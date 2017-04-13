#ifndef TYPE_H
#define TYPE_H

#include "string.h"

#include <stdlib.h>
#include <string.h>

enum type {
    T_VOID,
    T_CHAR,
    T_SHORT,
    T_INT,
    T_LONG,
    T_FLOAT,
    T_DOUBLE,
    T_LDOUBLE,
    T_POINTER,
    T_FUNCTION,
    T_ARRAY,
    T_STRUCT,
    T_UNION
};

/*
 * Internal representation of a type.
 *
 * Optimize for space and treat as a value type. This should fit in 32
 * bits, and be able to encode basic types such as:
 *
 *     int
 *     const int * volatile
 *     unsigned short const * const.
 *
 * For function, array, aggregate, and deeper pointer types, the type is
 * encoded in an opaque structure referenced by ref. All other types are
 * completely represented by this object, and have ref value 0.
 */
typedef struct {
    int type : 8;
    unsigned int is_unsigned : 1;
    unsigned int is_const : 1;
    unsigned int is_volatile : 1;
    unsigned int is_pointer : 1;
    unsigned int is_pointer_const : 1;
    unsigned int is_pointer_volatile : 1;
    unsigned int : 2;
    int ref : 16;
} Type;

/* Type can be overridden to mean pointer. */
#define type_of(t) ((t).is_pointer ? T_POINTER : (t).type)

/* Reflect semantics given in standardese. */
#define is_object(t) (!is_function(t))
#define is_function(t) (type_of(t) == T_FUNCTION)
#define is_struct_or_union(t) (is_struct(t) || is_union(t))
#define is_char(t) (type_of(t) == T_CHAR)
#define is_int(t) (type_of(t) == T_INT)
#define is_integer(t) (type_of(t) >= T_CHAR && type_of(t) <= T_LONG)
#define is_signed(t) (!is_unsigned(t) && is_integer(t))
#define is_unsigned(t) (!(t).is_pointer && (t).is_unsigned)
#define is_pointer(t) (type_of(t) == T_POINTER)
#define is_real(t) (type_of(t) >= T_FLOAT && type_of(t) <= T_LDOUBLE)
#define is_float(t) (type_of(t) == T_FLOAT)
#define is_double(t) (type_of(t) == T_DOUBLE)
#define is_long_double(t) (type_of(t) == T_LDOUBLE)
#define is_arithmetic(t) (type_of(t) >= T_CHAR && type_of(t) <= T_LDOUBLE)
#define is_scalar(t) (type_of(t) >= T_CHAR && type_of(t) <= T_POINTER)
#define is_aggregate(t) (is_array(t) || is_struct_or_union(t))
#define is_void(t) (type_of(t) == T_VOID)
#define is_array(t) (type_of(t) == T_ARRAY)
#define is_struct(t) (type_of(t) == T_STRUCT)
#define is_union(t) (type_of(t) == T_UNION)
#define is_const(t) ((t).is_pointer ? (t).is_pointer_const : (t).is_const)
#define is_volatile(t) ( \
    (t).is_pointer ? (t).is_pointer_volatile : (t).is_volatile)

/* Statically initialized, unqualified instances of common types. */
extern const Type
    basic_type__void,
    basic_type__char,
    basic_type__short,
    basic_type__int,
    basic_type__long,
    basic_type__unsigned_char,
    basic_type__unsigned_short,
    basic_type__unsigned_int,
    basic_type__unsigned_long,
    basic_type__float,
    basic_type__double,
    basic_type__long_double;

/*
 * Represent a function parameter, or struct or union member.
 *
 * Offset is used for aggregate member position. Fields additionally
 * have field_offset and field_width to specify location in containing
 * type.
 *
 * struct {
 *     char a;        // offset = 0
 *     int  b : 5;    // offset = 4, field_width = 5, field_offset = 0
 *     int  c : 7;    // offset = 4, field_width = 7, field_offset = 5
 * }
 *
 * Symbol is used for function parameters, pointing to the actual
 * instances registered in the definition.
 */
struct member {
    String name;
    Type type;
    size_t offset;
    short field_width;
    short field_offset;
    struct symbol *sym;
};

/* Get the number of struct or union members, or function parameters. */
int nmembers(Type type);

/* Return the n-th struct or union member, or function parameter. */
struct member *get_member(Type type, int n);

/* Get pointer target, function return type, or array element type. */
Type type_next(Type type);

/* A function takes variable arguments if last parameter is '...'. */
int is_vararg(Type type);

/*
 * Determine whether type is a variable length array, with size only
 * available at runtime.
 */
int is_vla(Type type);

/*
 * Determine whether struct or union type contains a flexible array
 * member.
 */
int is_flexible(Type type);

/* Return size of type. If indirection, return size of tagged type. */
size_t size_of(Type type);

/* Alignment in bytes. */
size_t type_alignment(Type type);

/* Returns 1 if types are equal, 0 otherwise. */
int type_equal(Type l, Type r);

/*
 * Find a common real type between operands used in an expression,
 * giving the type of the result.
 */
Type usual_arithmetic_conversion(Type t1, Type t2);

/* Print type to stream, returning number of characters written. */
int fprinttype(FILE *stream, Type type);

#endif
