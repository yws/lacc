#include "dot.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_BUF_LEN 256
#define BUFFERS 4

static char buffer[BUFFERS][MAX_BUF_LEN];

static FILE *stream;

/*
 * Return a buffer which can be used to write string representation of
 * variable or symbol.
 */
static char *get_buffer(void)
{
    static int i;

    i = (i + 1) % BUFFERS;
    return buffer[i];
}

static const char *sanitize(const struct symbol *sym)
{
    const char *label;
    char *buffer;
    assert(sym->symtype == SYM_LABEL);

    label = sym_name(sym);
    buffer = get_buffer();
    strncpy(
        buffer,
        (label[0] == '.') ? label + 1 : label,
        MAX_BUF_LEN);
    return buffer;
}

static const char *escape(const struct symbol *sym)
{
    const char *label;
    char *buffer;
    assert(sym->symtype == SYM_LABEL);

    label = sym_name(sym);
    buffer = get_buffer();
    if (label[0] == '.') {
        buffer[0] = '\\';
        strncpy(buffer + 1, label, MAX_BUF_LEN - 1);
    } else {
        strncpy(buffer, label, MAX_BUF_LEN);
    }

    return buffer;
}

static char *vartostr(const struct var var)
{
    int n = 0;
    char *buffer = get_buffer();

    switch (var.kind) {
    case IMMEDIATE:
        switch (type_of(var.type)) {
        default: assert(0);
        case T_POINTER:
            if (var.symbol) {
                assert(var.symbol->symtype == SYM_STRING_VALUE);
                if (var.offset) {
                    n = sprintf(buffer, "$%s+%lu",
                        sym_name(var.symbol), var.offset);
                } else {
                    n = sprintf(buffer, "$%s", sym_name(var.symbol));
                }
                break;
            }
        case T_CHAR:
        case T_SHORT:
        case T_INT:
        case T_LONG:
            if (is_unsigned(var.type)) {
                n = sprintf(buffer, "%lu", var.imm.u);
            } else {
                n = sprintf(buffer, "%ld", var.imm.i);
            }
            break;
        case T_FLOAT:
            n = sprintf(buffer, "%ff", var.imm.f);
            break;
        case T_DOUBLE:
            n = sprintf(buffer, "%f", var.imm.d);
            break;
        case T_LDOUBLE:
            n = sprintf(buffer, "%LfL", var.imm.ld);
            break;
        case T_ARRAY:
            assert(var.symbol && var.symbol->symtype == SYM_STRING_VALUE);
            n = sprintf(buffer, "\\\"%s\\\"",
                str_raw(var.symbol->string_value));
            break;
        }
        break;
    case DIRECT:
        if (var.offset) {
            n = sprintf(buffer, "*(&%s + %lu)",
                sym_name(var.symbol), var.offset);
        } else {
            n = sprintf(buffer, "%s", sym_name(var.symbol));
        }
        break;
    case ADDRESS:
        if (var.offset) {
            n = sprintf(buffer, "(&%s + %lu)",
                sym_name(var.symbol), var.offset);
        } else {
            n = sprintf(buffer, "&%s", sym_name(var.symbol));
        }
        break;
    case DEREF:
        if (var.offset) {
            n = sprintf(buffer, "*(%s + %lu)",
                sym_name(var.symbol), var.offset);
        } else {
            n = sprintf(buffer, "*%s", sym_name(var.symbol));
        }
        break;
    }

    if (is_field(var)) {
        sprintf(buffer + n, ":%d:%d", var.field_offset, var.field_width);
    }

    return buffer;
}

static void dot_print_expr(struct expression expr)
{
    switch (expr.op) {
    case IR_OP_CAST:
        if (type_equal(expr.type, expr.l.type)) {
            fprintf(stream, "%s", vartostr(expr.l));
        } else {
            fputc('(', stream);
            fprinttype(stream, expr.type);
            fprintf(stream, ") %s", vartostr(expr.l));
        }
        break;
    case IR_OP_CALL:
        fprintf(stream, "call %s", vartostr(expr.l));
        break;
    case IR_OP_VA_ARG:
        fprintf(stream, "va_arg(%s, ", vartostr(expr.l));
        fprinttype(stream, expr.type);
        fputc(')', stream);
        break;
    case IR_OP_NOT:
        fprintf(stream, "~%s", vartostr(expr.l));
        break;
    case IR_OP_NEG:
        fprintf(stream, "-%s", vartostr(expr.l));
        break;
    case IR_OP_ADD:
        fprintf(stream, "%s + %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_SUB:
        fprintf(stream, "%s - %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_MUL:
        fprintf(stream, "%s * %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_DIV:
        fprintf(stream, "%s / %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_MOD:
        fprintf(stream, "%s %% %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_AND:
        fprintf(stream, "%s & %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_OR:
        fprintf(stream, "%s \\| %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_XOR:
        fprintf(stream, "%s ^ %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_SHL:
        fprintf(stream, "%s \\<\\< %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_SHR:
        fprintf(stream, "%s \\>\\> %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_EQ:
        fprintf(stream, "%s == %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_NE:
        fprintf(stream, "%s != %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_GE:
        fprintf(stream, "%s \\>= %s", vartostr(expr.l), vartostr(expr.r));
        break;
    case IR_OP_GT:
        fprintf(stream, "%s \\> %s", vartostr(expr.l), vartostr(expr.r));
        break;
    }
}

static void dot_print_node(struct block *node)
{
    int i;
    struct statement s;

    if (node->color == BLACK)
        return;

    node->color = BLACK;
    fprintf(stream, "\t%s [label=\"{ %s",
        sanitize(node->label), escape(node->label));

    for (i = 0; i < array_len(&node->code); ++i) {
        s = array_get(&node->code, i);
        switch (s.st) {
        case IR_ASSIGN:
            fprintf(stream, " | %s = ", vartostr(s.t));
            dot_print_expr(s.expr);
            break;
        case IR_PARAM:
            fputs(" | param ", stream);
            dot_print_expr(s.expr);
            break;
        case IR_VA_START:
            fputs(" | va_start(", stream);
            dot_print_expr(s.expr);
            fputs(")", stream);
            break;
        case IR_EXPR:
            fputs(" | ", stream);
            dot_print_expr(s.expr);
            break;
        case IR_VLA_ALLOC:
            fprintf(stream, " | vla_alloc %s:%s (",
                vartostr(s.t),
                vartostr(var_direct(s.t.symbol->vla_address)));
            dot_print_expr(s.expr);
            fputs(")", stream);
            break;
        }
    }

    if (!node->jump[0] && !node->jump[1]) {
        if (node->has_return_value) {
            fputs(" | return ", stream);
            dot_print_expr(node->expr);
        }
        fputs(" }\"];\n", stream);
    } else if (node->jump[1]) {
        assert(node->jump[0]);
        fputs(" | if ", stream);
        dot_print_expr(node->expr);
        fprintf(stream, " goto %s", escape(node->jump[1]->label));
        fprintf(stream, " }\"];\n");
        dot_print_node(node->jump[0]);
        dot_print_node(node->jump[1]);
        fprintf(stream, "\t%s:s -> %s:n;\n",
            sanitize(node->label), sanitize(node->jump[0]->label));
        fprintf(stream, "\t%s:s -> %s:n;\n",
            sanitize(node->label), sanitize(node->jump[1]->label));
    } else {
        assert(node->jump[0]);
        assert(!node->jump[1]);
        fprintf(stream, " }\"];\n");
        dot_print_node(node->jump[0]);
        fprintf(stream, "\t%s:s -> %s:n;\n",
            sanitize(node->label), sanitize(node->jump[0]->label));
    }
}

void dot_init(FILE *output)
{
    assert(!stream);
    assert(output);
    stream = output;
}

void dotgen(struct definition *def)
{
    assert(stream);
    fprintf(stream, "digraph {\n");
    fprintf(stream, "\tnode [fontname=\"Courier_New\",fontsize=10,"
                    "style=\"setlinewidth(0.1)\",shape=record];\n");
    fprintf(stream, "\tedge [fontname=\"Courier_New\",fontsize=10,"
                    "style=\"setlinewidth(0.1)\"];\n");
    if (is_function(def->symbol->type)) {
        fprintf(stream, "\tlabel=\"%s\"\n", sym_name(def->symbol));
        fprintf(stream, "\tlabelloc=\"t\"\n");
    }

    dot_print_node(def->body);
    fprintf(stream, "}\n");
}
