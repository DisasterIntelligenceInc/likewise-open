/*
 * Copyright (c) Likewise Software.  All rights Reserved.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the license, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
 * General Public License for more details.  You should have received a copy
 * of the GNU Lesser General Public License along with this program.  If
 * not, see <http://www.gnu.org/licenses/>.
 *
 * LIKEWISE SOFTWARE MAKES THIS SOFTWARE AVAILABLE UNDER OTHER LICENSING
 * TERMS AS WELL.  IF YOU HAVE ENTERED INTO A SEPARATE LICENSE AGREEMENT
 * WITH LIKEWISE SOFTWARE, THEN YOU MAY ELECT TO USE THE SOFTWARE UNDER THE
 * TERMS OF THAT SOFTWARE LICENSE AGREEMENT INSTEAD OF THE TERMS OF THE GNU
 * LESSER GENERAL PUBLIC LICENSE, NOTWITHSTANDING THE ABOVE NOTICE.  IF YOU
 * HAVE QUESTIONS, OR WISH TO REQUEST A COPY OF THE ALTERNATE LICENSING
 * TERMS OFFERED BY LIKEWISE SOFTWARE, PLEASE CONTACT LIKEWISE SOFTWARE AT
 * license@likewisesoftware.com
 */

/*
 * Module Name:
 *
 *        type.h
 *
 * Abstract:
 *
 *        Type specification API (public header)
 *
 * Authors: Brian Koropoff (bkoropoff@likewisesoftware.com)
 *
 */
#ifndef __LWMSG_TYPE_H__
#define __LWMSG_TYPE_H__

#include <lwmsg/status.h>
#include <lwmsg/buffer.h>
#include <lwmsg/common.h>

#include <stddef.h>
#include <stdlib.h>
#include <inttypes.h>

/**
 * @defgroup types Types
 * @ingroup public
 * @brief Describe the structure of marshallable types
 *
 * This module provides the means to describe the layout of C types
 * such that they can be processed by the <tt>LWMsg</tt> marshaller. A type
 * specification consists of a global, statically-initialized array
 * of the #LWMsgTypeSpec type.  The initializer should consist of
 * a set of braces enclosing a comma-separated list of type specification
 * macros.
 * 
 * In order to allow for automated marshalling, a type specification
 * must include additional information which is not explicit in the
 * C type definition, such as how to determine the dynamic length
 * of arrays and pointer referents. Consider the following example
 * of a simple C structure and its <tt>LWMsg</tt> specification.
 *
 * @code
 * typedef struct _Foo
 * {
 *     char* name;
 *     size_t length;
 *     long* numbers;
 * } Foo;
 *
 * LWMsgTypeSpec foo_spec[] =
 * {
 *     // Begin layout of "Foo" structure
 *     LWMSG_STRUCT_BEGIN(Foo),
 *     // Foo has a member "name" which is a pointer to a null-terminated string
 *     LWMSG_MEMBER_PSTR(Foo, name),
 *     // Foo has a member "length" which should be marshalled as an unsigned 32-bit integer
 *     LWMSG_MEMBER_UINT32(Foo, length),
 *     // Foo has a member "numbers" which is a pointer
 *     LWMSG_MEMBER_POINTER_BEGIN(Foo, numbers),
 *     // The pointer points to one or more longs which are marshalled as signed 64-bit integers
 *     LWMSG_INT64(long),
 *     // End the member "numbers" pointer
 *     LWMSG_POINTER_END,
 *     // Indicate that the dynamic length of "numbers" is the value of "length"
 *     LWMSG_ATTR_LENGTH_MEMBER(Foo, length),
 *     // End the "Foo" structure
 *     LWMSG_STRUCT_END,
 *     // End the type specification
 *     LWMSG_TYPE_END
 * };
 * @endcode
 *
 * Type specifications have the following restrictions compared to C type definitions:
 * - The length of arrays and pointer referents must be specified:
 *     - As a static length with #LWMSG_ATTR_LENGTH_STATIC()
 *     - As the value of a <i>previous</i> integer structure member with #LWMSG_ATTR_LENGTH_MEMBER()
 *     - As being implicit through nul-termination with #LWMSG_ATTR_STRING
 * - The active arm of unions must be specified through a discriminator:
 *     - Each union arm must be marked with an integer tag with #LWMSG_ATTR_TAG()
 *     - Each instance of a union type must be correlated with a <i>previous</i> integer
 *       structure member with #LWMSG_ATTR_DISCRIM()
 *
 * Type specifications have the following leniencies compared to C type definitions:
 * - The order in which structure members are specified may differ from their
 *   order in the C definition, with the expection of flexible array members.
 *   Members will be marshalled in the order of the type specification.
 * - The specified width of an integer type may differ from the C type.  The
 *   integer will be marshalled to the specified size, with widening or truncating
 *   conversions applied automatically.  <b>Caution</b>: truncating conversions which
 *   cause overflow or underflow will generate runtime errors.
 */

/* @{ */

#ifndef DOXYGEN
struct LWMsgDataContext;
struct LWMsgContext;

typedef enum LWMsgKind
{
    LWMSG_KIND_NONE,
    /* Atomic integral type */
    LWMSG_KIND_INTEGER,
    LWMSG_KIND_STRUCT,
    LWMSG_KIND_UNION,
    LWMSG_KIND_ARRAY,
    LWMSG_KIND_POINTER,
    LWMSG_KIND_CUSTOM,
    LWMSG_KIND_VOID
} LWMsgKind;

typedef enum LWMsgArrayTermination
{
    /* Static length */
    LWMSG_TERM_STATIC,
    /* Zero-terminated */
    LWMSG_TERM_ZERO,
    /* Specified in member of dominating node */
    LWMSG_TERM_MEMBER
} LWMsgArrayTermination;

typedef enum LWMsgByteOrder
{
    LWMSG_LITTLE_ENDIAN,
    LWMSG_BIG_ENDIAN
} LWMsgByteOrder;
#endif

/**
 * @brief Indicate signed or unsigned status
 *
 * Indicates whether an integer type is signed or unsigned.
 */
typedef enum LWMsgSignage
{
    /** Signed */
    LWMSG_SIGNED,
    /** Unsigned */
    LWMSG_UNSIGNED
} LWMsgSignage;

typedef struct LWMsgTypeAttrs
{
    size_t custom;
    size_t range_low;
    size_t range_high;
    unsigned nonnull:1;
    unsigned aliasable:1;
    unsigned sensitive:1;
} LWMsgTypeAttrs;

/**
 * @brief Custom marshal function
 *
 * A callback function type which implements marshalling logic for
 * a custom marshaller type.
 *
 * @param context the marshalling context
 * @param object_size the in-memory size of the object to marshal, or 0 if unknown
 * @param object the in-memory object to marshal
 * @param attrs attributes of the type to marshal
 * @param buffer the marshalling buffer with the cursor set to where the 
 * serialized representation should be written
 * @param data the user data pointer specified to LWMSG_CUSTOM() or LWMSG_MEMBER_CUSTOM()
 * in the type specification
 * @lwmsg_status
 * @lwmsg_success
 * @lwmsg_etc{implementation-specific error}
 * @lwmsg_endstatus
 */
typedef LWMsgStatus (*LWMsgCustomMarshalFunction) (
    struct LWMsgDataContext* context,
    size_t object_size,
    void* object,
    LWMsgTypeAttrs* attrs,
    LWMsgBuffer* buffer,
    void* data
    );

/**
 * @brief Custom unmarshal function
 *
 * A callback function type which implements unmarshalling logic for
 * a custom marshaller type.
 *
 * @param context the marshalling context
 * @param buffer the marshalling buffer with the cursor set to the start of 
 * the serialized representation
 * @param object_size the in-memory size of the object to unmarshal, or 0 if known
 * @param attrs attributes of the type to unmarshal
 * @param object the location to unmarshal to
 * @param data the user data pointer specified to LWMSG_CUSTOM() or LWMSG_MEMBER_CUSTOM()
 * in the type specification
 * @lwmsg_status
 * @lwmsg_success
 * @lwmsg_etc{implementation-specific error}
 * @lwmsg_endstatus
 */
typedef LWMsgStatus (*LWMsgCustomUnmarshalFunction) (
    struct LWMsgDataContext* context,
    LWMsgBuffer* buffer,
    size_t object_size,
    LWMsgTypeAttrs* attrs,
    void* object,
    void* data
    );

/**
 * @brief Custom free function
 *
 * A callback function type which frees an instance of a custom type.
 *
 * @param context the marshalling context
 * @param object_size the in-memory size of the object to free, or 0 if unknown
 * @param attrs attributes of the type to free
 * @param object the address of the object to free
 * @param data the user data pointer specified to LWMSG_CUSTOM() or LWMSG_MEMBER_CUSTOM()
 * in the type specification
 */
typedef void (*LWMsgCustomFreeFunction) (
    const struct LWMsgContext* context,
    size_t object_size,
    LWMsgTypeAttrs* attr,
    void* object,
    void* data
    );

/**
 * @brief Print callback function
 *
 * A callback function used to print text by #lwmsg_data_print_graph()
 *
 * @param text the text to print
 * @param length the length of text
 * @param data a custom data pointer
 * @lwmsg_status
 * @lwmsg_success
 * @lwmsg_etc{implementation-specific error}
 * @lwmsg_endstatus
 */
typedef LWMsgStatus
(*LWMsgDataPrintFunction) (
    const char* text,
    size_t length,
    void* data
    );

/**
 * @brief Custom print function
 *
 * A callback function type which prints the representation of a custom type,
 * using the provided type printing callback.
 *
 * @param context the marshalling context
 * @param object_size the in-memory size of the object to free, or 0 if unknown
 * @param object the address of the object to print
 * @param attrs attributes of the type to print
 * @param data the user data pointer specified to LWMSG_CUSTOM() or LWMSG_MEMBER_CUSTOM()
 * in the type specification
 * @param print the type print callback
 * @param print_data the user data pointer to pass to the type print callback
 */
typedef LWMsgStatus (*LWMsgCustomPrintFunction) (
    const struct LWMsgContext* context,
    size_t object_size,
    void* object,
    LWMsgTypeAttrs* attr,
    void* data,
    LWMsgDataPrintFunction print,
    void* print_data
    );

/**
 * @brief Custom marshaller type class
 *
 * Describes a custom marshaller type which may be used with
 * LWMSG_CUSTOM() or LWMSG_MEMBER_CUSTOM() in a type specification
 *
 */
typedef struct LWMsgCustomTypeClass
{
    /** @brief Whether the type should be considered a pointer */
    LWMsgBool is_pointer;
    /** @brief Marshal callback function */
    LWMsgCustomMarshalFunction marshal;
    /** @brief Unmarshal callback function */
    LWMsgCustomUnmarshalFunction unmarshal;
    /** @brief Free callback function */
    LWMsgCustomFreeFunction free;
    /** @brief Print callback function */
    LWMsgCustomPrintFunction print;
} LWMsgCustomTypeClass;

/**
 * @brief Custom data verification function
 *
 * A callback function which performs verification of
 * in-memory data immediately before marshalling or immediately
 * after unmarshalling.
 *
 * @param context the marshalling context
 * @param unmarshalling true whether the operation being performed is unmarshalling
 * @param object_size the size of the object to verify in bytes
 * @param object the object to verify
 * @param data the user data pointer given to LWMSG_ATTR_VERIFY()
 * @lwmsg_status
 * @lwmsg_success
 * @lwmsg_code{MALFORMED, the object did not pass verification}
 * @lwmsg_endstatus
 */
typedef LWMsgStatus (*LWMsgVerifyFunction) (
    struct LWMsgDataContext* context,
    LWMsgBool unmarshalling,
    size_t object_size,
    void* object,
    void *data
    );

/**
 * @brief Marshaller type specification
 *
 * The fundamental type used to represent a type specification.
 * This is considered an implementation detail.
 * @hideinitializer
 */
typedef size_t const LWMsgTypeSpec;

#ifndef DOXYGEN
typedef enum LWMsgTypeDirective
{
        LWMSG_CMD_END,
        LWMSG_CMD_STRUCT,
        LWMSG_CMD_UNION,
        LWMSG_CMD_INTEGER,
        LWMSG_CMD_POINTER,
        LWMSG_CMD_ARRAY,
        LWMSG_CMD_TYPESPEC,
        LWMSG_CMD_TERMINATION,
        LWMSG_CMD_TAG,
        LWMSG_CMD_DISCRIM,
        LWMSG_CMD_CUSTOM,
        LWMSG_CMD_VERIFY,
        LWMSG_CMD_RANGE,
        LWMSG_CMD_NOT_NULL,
        LWMSG_CMD_CUSTOM_ATTR,
        LWMSG_CMD_VOID,
        LWMSG_CMD_ENCODING,
        LWMSG_CMD_SENSITIVE,
        LWMSG_FLAG_MEMBER = 0x10000,
        LWMSG_FLAG_META = 0x20000,
        LWMSG_FLAG_DEBUG = 0x40000
} LWMsgTypeDirective;

#define LWMSG_CMD_MASK 0xFFFF
#define _TYPEARG(_v_) ((size_t) (_v_))
#define _TYPECMD(_c_) ((size_t) ((_c_) | _TYPEFLAG_DEBUG)) _TYPEDEBUG
#define _TYPECMD_TYPE(_c_, _t_) ((size_t) ((_c_) | _TYPEFLAG_META | _TYPEFLAG_DEBUG)) _TYPEMETA(_t_) _TYPEDEBUG
#define _TYPECMD_MEMBER(_c_, _t_, _m_) ((size_t) ((_c_) | LWMSG_FLAG_MEMBER | _TYPEFLAG_META | _TYPEFLAG_DEBUG)) _TYPEMETA_MEMBER(_t_, _m_) _TYPEDEBUG

#ifdef LWMSG_SPEC_META
#define _TYPEFLAG_META (LWMSG_FLAG_META)
#define _TYPEMETA(type) ,_TYPEARG(#type)
#define _TYPEMETA_MEMBER(type, field) ,_TYPEARG(#field)
#else
#define _TYPEFLAG_META (0)
#define _TYPEMETA(type)
#define _TYPEMETA_MEMBER(type, field)
#endif

#ifdef LWMSG_SPEC_DEBUG
#define _TYPEFLAG_DEBUG (LWMSG_FLAG_DEBUG)
#define _TYPEDEBUG ,_TYPEARG(__FILE__), _TYPEARG(__LINE__)
#else
#define _TYPEFLAG_DEBUG (0)
#define _TYPEDEBUG
#endif

#endif /* DOXYGEN */

/**
 * @brief Specify an empty type
 *
 * Specifies an empty (zero-length) type.  This is primarily useful
 * for indicating empty arms of a union.
 */
#define LWMSG_VOID \
    _TYPECMD(LWMSG_CMD_VOID)

/**
 * @brief Reference another type specification
 *
 * References another type specification, treating it as though it
 * were inserted in place into the type specification currently
 * being defined.  This mechanism may be used to avoid repeating
 * definitions for common types.
 *
 * @param spec the #LWMsgTypeSpec[] to reference
 * @hideinitializer
 */
#define LWMSG_TYPESPEC(spec)                    \
    _TYPECMD(LWMSG_CMD_TYPESPEC),               \
        _TYPEARG(spec)

/**
 * @brief Reference another type specification as a member
 *
 * Defines a member of a structure or union by referencing a
 * separate type specification.  This mechanism may be used to avoid
 * repeating definitions for common types.
 *
 * @param type the name of the containing type
 * @param field the name of the member
 * @param spec the #LWMsgTypeSpec[] which specifies the member type
 * @hideinitializer
 */
#define LWMSG_MEMBER_TYPESPEC(type, field, spec)                      \
    _TYPECMD_MEMBER(LWMSG_CMD_TYPESPEC, type, field),                 \
        _TYPEARG(sizeof(((type*)0)->field)),                          \
        _TYPEARG(offsetof(type, field)),                              \
        _TYPEARG(spec)

/**
 * @brief End a type specification
 *
 * Marks the end of a type specification.  All type specifications
 * must be terminated with this macro.
 *
 * @hideinitializer
 */
#define LWMSG_TYPE_END \
    _TYPECMD(LWMSG_CMD_END)

/**
 * @brief End a structure specification
 *
 * Marks the end of a structure specification started with 
 * #LWMSG_STRUCT_BEGIN() or #LWMSG_MEMBER_STRUCT_BEGIN().
 *
 * @hideinitializer
 */
#define LWMSG_STRUCT_END \
    _TYPECMD(LWMSG_CMD_END)

/**
 * @brief End a union specification
 *
 * Marks the end of a union specification started with 
 * #LWMSG_UNION_BEGIN() or #LWMSG_MEMBER_UNION_BEGIN().
 *
 * @hideinitializer
 */
#define LWMSG_UNION_END \
    _TYPECMD(LWMSG_CMD_END)

/**
 * @brief End a pointer specification
 *
 * Marks the end of a pointer specification started with 
 * #LWMSG_POINTER_BEGIN() or #LWMSG_MEMBER_POINTER_BEGIN().
 *
 * @hideinitializer
 */
#define LWMSG_POINTER_END \
    _TYPECMD(LWMSG_CMD_END)

/**
 * @brief End an array specification
 *
 * Marks the end of an array specification started with 
 * #LWMSG_MEMBER_ARRAY_BEGIN().
 *
 * @hideinitializer
 */
#define LWMSG_ARRAY_END \
    _TYPECMD(LWMSG_CMD_END)

/**
 * @brief Begin a structure definition
 *
 * Begins the definition of a structure within a type specification.
 * The end of the definition must be marked with LWMSG_STRUCT_END
 *
 * @param type the name of the structure type
 * @hideinitializer
 */
#define LWMSG_STRUCT_BEGIN(type)               \
    _TYPECMD_TYPE(LWMSG_CMD_STRUCT, type),     \
        _TYPEARG(sizeof(type))

/**
 * @brief Begin a structure definition as a member
 *
 * Begins the definition of a structure which is an inline member
 * of the structure or union currently being defined. The end of the
 * definition must be marked with LWMSG_STRUCT_END.
 *
 * @param type the name of the containing structure or union type
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_STRUCT_BEGIN(type, field)        \
    _TYPECMD_MEMBER(LWMSG_CMD_STRUCT, type, field),   \
        _TYPEARG(sizeof(((type*)0)->field)),          \
        _TYPEARG(offsetof(type, field))
        

/**
 * @brief Begin a union definition
 *
 * Begins the definition of a union within a type specification.
 * The end of the definition must be marked with LWMSG_UNION_END
 *
 * @param type the name of the union type
 * @hideinitializer
 */
#define LWMSG_UNION_BEGIN(type)                \
    _TYPECMD_TYPE(LWMSG_CMD_UNION, type),      \
        _TYPEARG(sizeof(type))

/**
 * @brief Begin a union definition as a member
 *
 * Begins the definition of a union which is an inline member
 * of the structure or union currently being defined. The end of the
 * definition must be marked with LWMSG_UNION_END.
 *
 * @param type the name of the containing structure or union type
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_UNION_BEGIN(type, field)      \
    _TYPECMD_MEMBER(LWMSG_CMD_UNION, type, field), \
        _TYPEARG(sizeof(((type*)0)->field)),       \
        _TYPEARG(offsetof(type, field))

/**
 * @brief Define an integer member
 *
 * Defines an integer member of a struct or union.
 *
 * @param type the name of the containing structure or union type
 * @param field the name of the member
 * @param width the marshalled size of the integer in bytes
 * @param sign the signedness of the integer as a #LWMsgSignage
 * @hideinitializer
 */
#define LWMSG_MEMBER_INTEGER(type, field, width, sign)                 \
    _TYPECMD_MEMBER(LWMSG_CMD_INTEGER, type, field),                   \
        _TYPEARG(sizeof(((type*)0)->field)),                           \
        _TYPEARG(offsetof(type, field)),                               \
        _TYPEARG(width),                                               \
        _TYPEARG(sign)

/**
 * @brief Define an integer
 *
 * Defines an integer
 *
 * @param type the unmarshalled, C type of the integer
 * @param width the marshalled size of the integer in bytes
 * @param sign the signedness of the integer as a #LWMsgSignage
 * @hideinitializer
 */
#define LWMSG_INTEGER(type, width, sign)       \
    _TYPECMD_TYPE(LWMSG_CMD_INTEGER, type),    \
        _TYPEARG(sizeof(type)),                \
        _TYPEARG(width),                       \
        _TYPEARG(sign)

/**
 * @brief Indicate static length
 *
 * Indicates the static length of the immediately previous
 * array or pointer type in a type specification
 *
 * @param count the number of elements in the previous type
 * @hideinitializer
 */
#define LWMSG_ATTR_LENGTH_STATIC(count)  \
    _TYPECMD(LWMSG_CMD_TERMINATION),     \
        _TYPEARG(LWMSG_TERM_STATIC),     \
        _TYPEARG(count)

/**
 * @brief Indicate correlated length member
 *
 * Indicates that that length of the immediately previous
 * array or pointer member is equal to the value of the
 * specified integer member, which must be defined before
 * the pointer or array member.
 *
 * @param type the type of the containing structure
 * @param field the name of the field which contains the length
 * @hideinitializer
 */
#define LWMSG_ATTR_LENGTH_MEMBER(type, field)   \
    _TYPECMD(LWMSG_CMD_TERMINATION),            \
        _TYPEARG(LWMSG_TERM_MEMBER),            \
        _TYPEARG(offsetof(type, field)),        \
        _TYPEARG(sizeof(((type*)0)->field))

/**
 * @brief Indicate null termination
 *
 * Indicates that the immediately previous array or pointer
 * member has a length determined by null- or zero- termination.
 * @hideinitializer
 */
#define LWMSG_ATTR_ZERO_TERMINATED              \
    _TYPECMD(LWMSG_CMD_TERMINATION),            \
        _TYPEARG(LWMSG_TERM_ZERO)

/**
 * @brief Indicate string encoding
 *
 * Indicates that the immediately previous array or pointer
 * represents text in the specified encoding.  This is used
 * as a hint to #lwmsg_data_print_graph() to aid in debugging
 * and does not affect the behavior of the marshaller.
 * @hideinitializer
 */
#define LWMSG_ATTR_ENCODING(enc)                \
    _TYPECMD(LWMSG_CMD_ENCODING),               \
        _TYPEARG((enc))

/**
 * @brief Indicate sensitive information
 *
 * Indicates that the immediately previous type or member
 * contains sensitive information (e.g. a password) and should
 * not be displayed when printed with lwmsg_data_print_graph().
 * @hideinitializer
 */
#define LWMSG_ATTR_SENSITIVE                    \
    _TYPECMD(LWMSG_CMD_SENSITIVE)

/**
 * @brief Indicate C string
 *
 * Indicates that the immediately previous array or pointer
 * represents a plain C string.  That is, it is nul-terminated
 * and encoded in the program's current locale.
 * @hideinitializer
 */
#define LWMSG_ATTR_STRING \
    LWMSG_ATTR_ZERO_TERMINATED,                 \
    LWMSG_ATTR_ENCODING("")

/**
 * @brief Indicate union tag
 *
 * Indicates that the immediately previous member of a union
 * is associated with a particular integer tag.  All members
 * of a union must be marked with this attribute.
 * @param value the integer value associated with the member
 * @hideinitializer
 */
#define LWMSG_ATTR_TAG(value)                  \
    _TYPECMD(LWMSG_CMD_TAG),                   \
        _TYPEARG(value)

/**
 * @brief Indicate correlated union discriminator
 *
 * Indicates that the immediately previous member, which
 * must be a union type, has an active arm which is determined
 * by the tag stored in the specified discriminator member.
 * All uses of unions must be marked with this attribute.
 *
 * @param type the name of the containing structure
 * @param field the name of the member which holds the tag value
 * @hideinitializer
 */
#define LWMSG_ATTR_DISCRIM(type, field)         \
    _TYPECMD(LWMSG_CMD_DISCRIM),                \
        _TYPEARG(offsetof(type, field)),        \
        _TYPEARG(sizeof(((type*)0)->field))

/**
 * @brief Apply custom data verifier
 *
 * Applies a custom data verification function to the previous type
 * or member.  The function will be called with the in-memory form
 * of the data immediately before it is marshalled or immediately
 * after it is unmarshalled.
 *
 * Only one custom data verifier may be applied to a given
 * type or member.
 *
 * @param func the verifier function
 * @param data a constant user data pointer to pass to the function
 * @hideinitializer
 */
#define LWMSG_ATTR_VERIFY(func, data)           \
    _TYPECMD(LWMSG_CMD_VERIFY),                 \
        _TYPEARG(func),                         \
        _TYPEARG(data)


/**
 * @brief Constrain range of integer type
 *
 * Constrains the range of an integer type to the specified bounds.
 * Attempts to marshal or unmarshal data where the type exceeds these
 * bounds will result in an immediate error.
 *
 * @param low the lower bound of the range (inclusive)
 * @param high the upper bound of the range (inclusive)
 * @hideinitializer
 */
#define LWMSG_ATTR_RANGE(low, high)           \
    _TYPECMD(LWMSG_CMD_RANGE),                \
        _TYPEARG(low),                        \
        _TYPEARG(high)


/**
 * @brief Restrict pointer nullability
 *
 * Specifies that the previous type or member, which must be a pointer,
 * must not be NULL.  Attempts to marshal or unmarshal data where the
 * affected type or member is NULL will result in an immediate
 * error.
 *
 * @hideinitializer
 */
#define LWMSG_ATTR_NOT_NULL                     \
    _TYPECMD(LWMSG_CMD_NOT_NULL)

/**
 * @brief Define a pointer as a member
 *
 * Defines a pointer which is a member of the current structure
 * or union.  This must be followed by the definition of the
 * pointer's contents.  The end of the pointer definition
 * must be marked by LWMSG_POINTER_END.
 *
 * @param type the name of the containing structure
 * @param field the name of the pointer member
 * @hideinitializer
 */
#define LWMSG_MEMBER_POINTER_BEGIN(type, field)       \
    _TYPECMD_MEMBER(LWMSG_CMD_POINTER, type, field),  \
        _TYPEARG(offsetof(type, field))

/**
 * @brief Define a pointer
 *
 * Defines a pointer. This must be followed by the definition of
 * the pointer's contents.  The end of the pointer definition
 * must be marked by #LWMSG_POINTER_END.
 *
 * @hideinitializer
 */
#define LWMSG_POINTER_BEGIN                     \
    _TYPECMD(LWMSG_CMD_POINTER)

/**
 * @brief Define an array as a member
 *
 * Defines an array which is a member of the current structure
 * or union.  This must be followed by the definition of the
 * array's contents.  The end of the array definition
 * must be marked by LWMSG_ARRAY_END.
 *
 * An array, as opposed to a pointer, has contents which are
 * stored inline in the the containing structure or union.
 * That is, each element of the array is stored as if it were
 * itself a member.  This difference corresponds naturally
 * to the difference between * and [] types in C structures.
 *
 * @param type the name of the containing structure
 * @param field the name of the array member
 * @hideinitializer
 */
#define LWMSG_MEMBER_ARRAY_BEGIN(type, field)         \
    _TYPECMD_MEMBER(LWMSG_CMD_ARRAY, type, field),    \
        _TYPEARG(offsetof(type, field))

/**
 * @brief Define a custom type
 *
 * Defines a custom type with user-specified marshaller logic.
 *
 * @param tclass a constant pointer to the LWMsgCustomTypeClass structure
 * containing marshalling methods
 * @param tdata a constant pointer to arbitrary data which will be passed
 * to the marshalling methods
 * @hideinitializer
 */
#define LWMSG_CUSTOM(tclass, tdata)             \
    _TYPECMD(LWMSG_CMD_CUSTOM),                 \
        _TYPEARG(tclass),                       \
        _TYPEARG(tdata)

/**
 * @brief Define a custom type as a member
 *
 * Defines a custom type with user-specified marshaller logic as
 * a member of a structure or union.
 *
 * @param type the name of the containing type
 * @param field the name of the member
 * @param tclass a constant pointer to the LWMsgCustomTypeClass structure
 * containing marshalling methods
 * @param tdata a constant pointer to arbitrary data which will be passed
 * to the marshalling methods
 * @hideinitializer
 */
#define LWMSG_MEMBER_CUSTOM(type, field, tclass, tdata)        \
    _TYPECMD_MEMBER(LWMSG_CMD_CUSTOM, type, field),            \
        _TYPEARG(sizeof(((type*)0)->field)),                   \
        _TYPEARG(offsetof(type, field)),                       \
        _TYPEARG(tclass),                                      \
        _TYPEARG(tdata)

/**
 * @brief Apply custom type attribute
 *
 * Applies a custom attribute to the previous type or member,
 * which must be a custom type.  The bitwise or of all custom
 * attribute values will be made available to the marshal and
 * unmarshal functions for the custom type.
 *
 * @param value the value of the attribute to apply
 * @hideinitializer
 */
#define LWMSG_ATTR_CUSTOM(value)                \
    _TYPECMD(LWMSG_CMD_CUSTOM_ATTR),            \
        _TYPEARG(value)

/* Handy aliases for more complicated commands */

/**
 * @brief Define a signed 8-bit integer member
 *
 * Defines a signed 8-bit integer member of a struct or union.
 * This is a convenient shortcut for a full LWMSG_MEMBER_INTEGER()
 * invocation.
 *
 * @param type the name of the containing structure or union
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_INT8(type, field) LWMSG_MEMBER_INTEGER(type, field, 1, LWMSG_SIGNED)

/**
 * @brief Define a signed 16-bit integer member
 *
 * Defines a signed 16-bit integer member of a struct or union.
 * This is a convenient shortcut for a full LWMSG_MEMBER_INTEGER()
 * invocation.
 *
 * @param type the name of the containing structure or union
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_INT16(type, field) LWMSG_MEMBER_INTEGER(type, field, 2, LWMSG_SIGNED)

/**
 * @brief Define a signed 32-bit integer member
 *
 * Defines a signed 32-bit integer member of a struct or union.
 * This is a convenient shortcut for a full LWMSG_MEMBER_INTEGER()
 * invocation.
 *
 * @param type the name of the containing structure or union
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_INT32(type, field) LWMSG_MEMBER_INTEGER(type, field, 4, LWMSG_SIGNED)

/**
 * @brief Define a signed 64-bit integer member
 *
 * Defines a signed 64-bit integer member of a struct or union.
 * This is a convenient shortcut for a full LWMSG_MEMBER_INTEGER()
 * invocation.
 *
 * @param type the name of the containing structure or union
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_INT64(type, field) LWMSG_MEMBER_INTEGER(type, field, 8, LWMSG_SIGNED)

/**
 * @brief Define an unsigned 8-bit integer member
 *
 * Defines an unsigned 8-bit integer member of a struct or union.
 * This is a convenient shortcut for a full LWMSG_MEMBER_INTEGER()
 * invocation.
 *
 * @param type the name of the containing structure or union
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_UINT8(type, field) LWMSG_MEMBER_INTEGER(type, field, 1, LWMSG_UNSIGNED)

/**
 * @brief Define an unsigned 16-bit integer member
 *
 * Defines an unsigned 16-bit integer member of a struct or union.
 * This is a convenient shortcut for a full LWMSG_MEMBER_INTEGER()
 * invocation.
 *
 * @param type the name of the containing structure or union
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_UINT16(type, field) LWMSG_MEMBER_INTEGER(type, field, 2, LWMSG_UNSIGNED)

/**
 * @brief Define an unsigned 32-bit integer member
 *
 * Defines an unsigned 32-bit integer member of a struct or union.
 * This is a convenient shortcut for a full LWMSG_MEMBER_INTEGER()
 * invocation.
 *
 * @param type the name of the containing structure or union
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_UINT32(type, field) LWMSG_MEMBER_INTEGER(type, field, 4, LWMSG_UNSIGNED)

/**
 * @brief Define an unsigned 64-bit integer member
 *
 * Defines an unsigned 64-bit integer member of a struct or union.
 * This is a convenient shortcut for a full LWMSG_MEMBER_INTEGER()
 * invocation.
 *
 * @param type the name of the containing structure or union
 * @param field the name of the member
 * @hideinitializer
 */
#define LWMSG_MEMBER_UINT64(type, field) LWMSG_MEMBER_INTEGER(type, field, 8, LWMSG_UNSIGNED)

/**
 * @brief Define a signed 8-bit integer
 *
 * Defines a signed 8-bit integer type.  This is a convenient
 * shortcut for a full LWMSG_INTEGER() invocation.
 *
 * @param type the unmarshalled type
 * @hideinitializer
 */
#define LWMSG_INT8(type) LWMSG_INTEGER(type, 1, LWMSG_SIGNED)

/**
 * @brief Define a signed 16-bit integer
 *
 * Defines a signed 16-bit integer type.  This is a convenient
 * shortcut for a full LWMSG_INTEGER() invocation.
 *
 * @param type the unmarshalled type
 * @hideinitializer
 */
#define LWMSG_INT16(type) LWMSG_INTEGER(type, 2, LWMSG_SIGNED)

/**
 * @brief Define a signed 32-bit integer
 *
 * Defines a signed 32-bit integer type.  This is a convenient
 * shortcut for a full LWMSG_INTEGER() invocation.
 *
 * @param type the unmarshalled type
 * @hideinitializer
 */
#define LWMSG_INT32(type) LWMSG_INTEGER(type, 4, LWMSG_SIGNED)

/**
 * @brief Define a signed 64-bit integer
 *
 * Defines a signed 64-bit integer type.  This is a convenient
 * shortcut for a full LWMSG_INTEGER() invocation.
 *
 * @param type the unmarshalled type
 * @hideinitializer
 */
#define LWMSG_INT64(type) LWMSG_INTEGER(type, 8, LWMSG_SIGNED)

/**
 * @brief Define an unsigned 8-bit integer
 *
 * Defines an unsigned 8-bit integer type.  This is a convenient
 * shortcut for a full LWMSG_INTEGER() invocation.
 *
 * @param type the unmarshalled type
 * @hideinitializer
 */
#define LWMSG_UINT8(type) LWMSG_INTEGER(type, 1, LWMSG_UNSIGNED)

/**
 * @brief Define an unsigned 16-bit integer
 *
 * Defines an unsigned 16-bit integer type.  This is a convenient
 * shortcut for a full LWMSG_INTEGER() invocation.
 *
 * @param type the unmarshalled type
 * @hideinitializer
 */
#define LWMSG_UINT16(type) LWMSG_INTEGER(type, 2, LWMSG_UNSIGNED)

/**
 * @brief Define an unsigned 32-bit integer
 *
 * Defines an unsigned 32-bit integer type.  This is a convenient
 * shortcut for a full LWMSG_INTEGER() invocation.
 *
 * @param type the unmarshalled type
 * @hideinitializer
 */
#define LWMSG_UINT32(type) LWMSG_INTEGER(type, 4, LWMSG_UNSIGNED)

/**
 * @brief Define an unsigned 64-bit integer
 *
 * Defines an unsigned 64-bit integer type.  This is a convenient
 * shortcut for a full LWMSG_INTEGER() invocation.
 *
 * @param type the unmarshalled type
 * @hideinitializer
 */
#define LWMSG_UINT64(type) LWMSG_INTEGER(type, 8, LWMSG_UNSIGNED)

/**
 * @brief Define a member pointer to a string
 *
 * Defines a pointer to an 8-bit, null-terminated character string
 * as a member of a struct or union.  This is a convenient shortcut
 * for marshalling plain C strings, and is equivalent to the following:
 *
 * @code
 * LWMSG_MEMBER_POINTER_BEGIN(type, field),
 * LWMSG_UINT8(char),
 * LWMSG_POINTER_END,
 * LWMSG_ATTR_STRING
 * @endcode
 *
 * @param type the containing struct or union type
 * @param field the member of the struct or union
 * @hideinitializer
 */
#define LWMSG_MEMBER_PSTR(type, field)          \
    LWMSG_MEMBER_POINTER_BEGIN(type, field),    \
    LWMSG_UINT8(char),                          \
    LWMSG_POINTER_END,                          \
    LWMSG_ATTR_STRING

/**
 * @brief Define a pointer to a string
 *
 * Defines a pointer to an 8-bit, null-terminated character string.
 * This is a convenient shortcut for marshalling plain C strings,
 * and is equivalent to the following:
 *
 * @code
 * LWMSG_POINTER_BEGIN,
 * LWMSG_UINT8(char),
 * LWMSG_POINTER_END,
 * LWMSG_ATTR_STRING
 * @endcode
 *
 * @param type the containing struct or union type
 * @param field the member of the struct or union
 * @hideinitializer
 */
#define LWMSG_PSTR       \
    LWMSG_POINTER_BEGIN, \
    LWMSG_UINT8(char),   \
    LWMSG_POINTER_END,   \
    LWMSG_ATTR_STRING

/**
 * @brief Define a structure (compact)
 *
 * Defines a structure in a more compact fashion.  It is equivalent
 * to the following expanded form:
 *
 * @code
 * LWMSG_STRUCT_BEGIN(type),
 * ... ,
 * LWMSG_STRUCT_END
 * @endcode
 * 
 * @param type the C structure type
 * @param ... the contents of the structure specification
 * @hideinitializer
 */
#define LWMSG_STRUCT(type, ...)  \
    LWMSG_STRUCT_BEGIN(type),    \
    __VA_ARGS__,                 \
    LWMSG_STRUCT_END

/**
 * @brief Define a pointer (compact)
 *
 * Defines a pointer in a more compact fashion.  It is equivalent
 * to the following expanded form:
 *
 * @code
 * LWMSG_POINTER_BEGIN,
 * ... ,
 * LWMSG_POINTER_END
 * @endcode
 * 
 * @param ... the contents of the pointer specification
 * @hideinitializer
 */
#define LWMSG_POINTER(...) \
    LWMSG_POINTER_BEGIN,   \
    __VA_ARGS__,           \
    LWMSG_POINTER_END

/**
 * @brief Define a union (compact)
 *
 * Defines a union in a more compact fashion.  It is equivalent
 * to the following expanded form:
 *
 * @code
 * LWMSG_UNION_BEGIN(type),
 * ... ,
 * LWMSG_UNION_END
 * @endcode
 * 
 * @param type the C union type
 * @param ... the contents of the union specification
 * @hideinitializer
 */
#define LWMSG_UNION(type, ...)   \
    LWMSG_UNION_BEGIN(type),     \
    __VA_ARGS__,                 \
    LWMSG_UNION_END


/**
 * @brief Define a structure as a member (compact)
 *
 * Defines a structure as a member in a more compact fashion.
 * It is equivalent to the following expanded form:
 *
 * @code
 * LWMSG_MEMBER_STRUCT_BEGIN(type, field),
 * ... ,
 * LWMSG_STRUCT_END
 * @endcode
 * 
 * @param type the containing C type
 * @param field the member name
 * @param ... the contents of the structure specification
 * @hideinitializer
 */
#define LWMSG_MEMBER_STRUCT(type, field, ...)  \
    LWMSG_MEMBER_STRUCT_BEGIN(type, field),    \
    __VA_ARGS__,                               \
    LWMSG_STRUCT_END


/**
 * @brief Define a pointer as a member (compact)
 *
 * Defines a pointer as a member in a more compact fashion.
 * It is equivalent to the following expanded form:
 *
 * @code
 * LWMSG_MEMBER_POINTER_BEGIN(type, field),
 * ... ,
 * LWMSG_POINTER_END
 * @endcode
 * 
 * @param type the containing C type
 * @param field the member name
 * @param ... the contents of the structure specification
 * @hideinitializer
 */
#define LWMSG_MEMBER_POINTER(type, field, ...) \
    LWMSG_MEMBER_POINTER_BEGIN(type, field),   \
    __VA_ARGS__,                               \
    LWMSG_POINTER_END

/**
 * @brief Define a union as a member (compact)
 *
 * Defines a union as a member in a more compact fashion.
 * It is equivalent to the following expanded form:
 *
 * @code
 * LWMSG_MEMBER_UNION_BEGIN(type, field),
 * ... ,
 * LWMSG_UNION_END
 * @endcode
 * 
 * @param type the containing C type
 * @param field the member name
 * @param ... the contents of the union specification
 * @hideinitializer
 */
#define LWMSG_MEMBER_UNION(type, field, ...)   \
    LWMSG_MEMBER_UNION_BEGIN(type, field),     \
    __VA_ARGS__,                               \
    LWMSG_UNION_END

/**
 * @brief Define an array as a member (compact)
 *
 * Defines an array as a member in a more compact fashion.
 * It is equivalent to the following expanded form:
 *
 * @code
 * LWMSG_MEMBER_ARRAY_BEGIN(type, field),
 * ... ,
 * LWMSG_ARRAY_END
 * @endcode
 * 
 * @param type the containing C type
 * @param field the member name
 * @param ... the contents of the structure specification
 * @hideinitializer
 */
#define LWMSG_MEMBER_ARRAY(type, field, ...)   \
    LWMSG_MEMBER_ARRAY_BEGIN(type, field),     \
    __VA_ARGS__,                               \
    LWMSG_ARRAY_END

/*@}*/

#endif
