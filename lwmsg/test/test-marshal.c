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
 *        test-marshal.c
 *
 * Abstract:
 *
 *        Marshalling unit tests
 *
 * Authors: Brian Koropoff (bkoropoff@likewisesoftware.com)
 *
 */
#include <lwmsg/lwmsg.h>
#include <moonunit/interface.h>
#include <config.h>
#include <string.h>

#include "test-private.h"

static LWMsgContext* context;

static void
allocate_buffer(LWMsgBuffer* buffer)
{
    buffer->length = 2048;
    buffer->memory = malloc(buffer->length);
    buffer->cursor = buffer->memory;
    buffer->full = NULL;
    buffer->data = NULL;
}

static void
rewind_buffer(LWMsgBuffer* buffer)
{
    buffer->cursor = buffer->memory;
}

MU_FIXTURE_SETUP(marshal)
{
    MU_TRY(lwmsg_context_new(&context));
}

typedef struct basic_struct
{
    short foo;
    unsigned int len;
    long *long_ptr;
} basic_struct;

static
LWMsgStatus
basic_verify_foo(
    LWMsgContext* context,
    LWMsgBool unmarshalling,
    size_t object_size,
    void* object,
    void* data
    )
{
    short* fooptr = (short*) object;

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, object_size, sizeof(short));

    if (*fooptr != -42)
    {
        return LWMSG_STATUS_MALFORMED;
    }
    else
    {
        return LWMSG_STATUS_SUCCESS;
    }
}


static LWMsgTypeSpec basic_spec[] =
{
    LWMSG_STRUCT_BEGIN(basic_struct),
    LWMSG_MEMBER_INT16(basic_struct, foo), LWMSG_ATTR_VERIFY(basic_verify_foo, NULL),
    LWMSG_MEMBER_UINT32(basic_struct, len), LWMSG_ATTR_RANGE(1, 8),
    LWMSG_MEMBER_POINTER(basic_struct, long_ptr, LWMSG_INT64(long)), LWMSG_ATTR_NOT_NULL,
    LWMSG_ATTR_LENGTH_MEMBER(basic_struct, len),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, basic)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* -42 */
        0xFF, 0xD6,
        /* 2 */
        0x00, 0x00, 0x00, 0x02,
        /* pointer */
        0xFF,
        /* 1234 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xD2,
        /* 4321 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xE1
    };
    LWMsgTypeSpec* type = basic_spec;
    LWMsgBuffer buffer;
    basic_struct basic;
    basic_struct *out;
    long longs[2];

    allocate_buffer(&buffer);

    basic.foo = (short) -42;
    basic.len = 2;
    basic.long_ptr = longs;
    longs[0] = 1234;
    longs[1] = 4321;

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &basic, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT(basic.foo == out->foo);
    MU_ASSERT(basic.len == out->len);
    MU_ASSERT(basic.long_ptr[0] == out->long_ptr[0]);
    MU_ASSERT(basic.long_ptr[1] == out->long_ptr[1]);
}

MU_TEST(marshal, basic_verify_marshal_failure)
{
    LWMsgTypeSpec* type = basic_spec;
    LWMsgBuffer buffer;
    basic_struct basic;
    long longs[2];

    MU_EXPECT(MU_STATUS_EXCEPTION);

    allocate_buffer(&buffer);

    basic.foo = (short) 12;
    basic.len = 2;
    basic.long_ptr = longs;
    longs[0] = 1234;
    longs[1] = 4321;

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &basic, &buffer));
}

MU_TEST(marshal, basic_verify_unmarshal_failure)
{
    static const unsigned char bytes[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* 12 */
        0x00, 0x0C,
        /* 2 */
        0x00, 0x00, 0x00, 0x02,
        /* pointer */
        0xFF,
        /* 1234 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xD2,
        /* 4321 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xE1
    };

    LWMsgTypeSpec* type = basic_spec;
    LWMsgBuffer buffer = {0};
    basic_struct *out;

    MU_EXPECT(MU_STATUS_EXCEPTION);

    buffer.memory = buffer.cursor = (void*) bytes;
    buffer.length = sizeof(bytes);
    

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));
}

MU_TEST(marshal, basic_verify_range_failure)
{
    static const unsigned char bytes[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* -42 */
        0xFF, 0xD6,
        /* 9 */
        0x00, 0x00, 0x00, 0x09,
        /* pointer */
        0xFF,
        /* 1234 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0xD2,
        /* 4321 */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xE1
    };

    LWMsgTypeSpec* type = basic_spec;
    LWMsgBuffer buffer = {0};
    basic_struct *out;

    MU_EXPECT(MU_STATUS_EXCEPTION);

    buffer.memory = buffer.cursor = (void*) bytes;
    buffer.length = sizeof(bytes);
    

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));
}

MU_TEST(marshal, basic_verify_null_failure)
{
    static const unsigned char bytes[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* -42 */
        0xFF, 0xD6,
        /* 2 */
        0x00, 0x00, 0x00, 0x02,
        /* pointer is NULL */
        0x00
    };

    LWMsgTypeSpec* type = basic_spec;
    LWMsgBuffer buffer = {0};
    basic_struct *out;

    MU_EXPECT(MU_STATUS_EXCEPTION);

    buffer.memory = buffer.cursor = (void*) bytes;
    buffer.length = sizeof(bytes);
    

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));
}

typedef struct
{
    const char* foo;
    const char* bar;
} string_struct;

static LWMsgTypeSpec string_spec[] =
{
    LWMSG_STRUCT_BEGIN(string_struct),
    LWMSG_MEMBER_PSTR(string_struct, foo),
    LWMSG_MEMBER_PSTR(string_struct, bar),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, string)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* foo set */
        0xFF,
        /* foo length */
        0x00, 0x00, 0x00, 0x04,
        /* foo contents */
        'f', 'o', 'o', '\0',
        /* bar set */
        0xFF,
        /* bar length */
        0x00, 0x00, 0x00, 0x04,
        /* bar contents */
        'b', 'a', 'r', '\0'
    };

    LWMsgTypeSpec* type = string_spec;
    LWMsgBuffer buffer;
    string_struct strings;
    string_struct* out;

    strings.foo = "foo";
    strings.bar = "bar";
    
    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &strings, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_STRING, strings.foo, out->foo);
    MU_ASSERT_EQUAL(MU_TYPE_STRING, strings.bar, out->bar);
}

typedef struct
{
    unsigned int len;
    struct struct_array_inner
    {
        char a;
        char b;
    } *foo;
} struct_array_struct;

static LWMsgTypeSpec struct_array_spec[] =
{
    LWMSG_STRUCT_BEGIN(struct_array_struct),
    LWMSG_MEMBER_UINT32(struct_array_struct, len),
    LWMSG_MEMBER_POINTER_BEGIN(struct_array_struct, foo),
    LWMSG_STRUCT_BEGIN(struct struct_array_inner),
    LWMSG_MEMBER_INT8(struct struct_array_inner, a),
    LWMSG_MEMBER_INT8(struct struct_array_inner, b),
    LWMSG_STRUCT_END,
    LWMSG_POINTER_END,
    LWMSG_ATTR_LENGTH_MEMBER(struct_array_struct, len),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, struct_array)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* len = 2 */
        0x00, 0x00, 0x00, 0x02,
        /* foo set */
        0xFF,
        /* first struct */
        'a', 'b',
        /* second struct */
        'c', 'd'
    };

    LWMsgTypeSpec* type = struct_array_spec;
    LWMsgBuffer buffer;
    struct_array_struct structs;
    struct_array_struct *out;
    struct struct_array_inner inner[2] = { {'a', 'b'}, {'c', 'd'} };

    structs.len = 2;
    structs.foo = inner;
    
    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &structs, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT(structs.len == out->len);
    MU_ASSERT(structs.foo[0].a == out->foo[0].a);
    MU_ASSERT(structs.foo[0].b == out->foo[0].b);
    MU_ASSERT(structs.foo[1].a == out->foo[1].a);
    MU_ASSERT(structs.foo[1].b == out->foo[1].b);
}

typedef struct
{
    char ** strings;
} string_array_struct;

static LWMsgTypeSpec string_array_spec[] =
{
    LWMSG_STRUCT_BEGIN(string_array_struct),
    LWMSG_MEMBER_POINTER_BEGIN(string_array_struct, strings),
    LWMSG_PSTR,
    LWMSG_POINTER_END,
    LWMSG_ATTR_STRING,
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, string_array)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* strings = (set) */
        0xFF,
        /* (implicit) length of "strings" = 3 */
        0x00, 0x00, 0x00, 0x03,
        /* strings[0] = set */
        0xFF,
        /* strings[0] len */
        0x00, 0x00, 0x00, 0x04,
        /* strings[0] value */
        'f', 'o', 'o', '\0',
        /* strings[1] = set */
        0xFF,
        /* strings[1] len */
        0x00, 0x00, 0x00, 0x04,
        /* strings[1] value */
        'b', 'a', 'r', '\0',
        /* strings[2] = unset */
        0x00
    };

    LWMsgTypeSpec* type = string_array_spec;
    LWMsgBuffer buffer;
    string_array_struct strings;
    string_array_struct *out;
    char* inner[] = { "foo", "bar", NULL };

    strings.strings = (char**) inner;
    
    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &strings, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_STRING, strings.strings[0], out->strings[0]);
    MU_ASSERT_EQUAL(MU_TYPE_STRING, strings.strings[1], out->strings[1]);
    MU_ASSERT(out->strings[2] == NULL);

    lwmsg_context_free_graph(context, type, out);
}

MU_TEST(marshal, string_array_empty)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* strings = (set) */
        0xFF,
        /* (implicit) length of "strings" = 1 */
        0x00, 0x00, 0x00, 0x01,
        /* strings[0] = unset */
        0x00
    };

    LWMsgTypeSpec* type = string_array_spec;
    LWMsgBuffer buffer;
    string_array_struct strings;
    string_array_struct *out;
    char* inner[] = { NULL };

    strings.strings = (char**) inner;

    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &strings, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_POINTER, strings.strings[0], out->strings[0]);

    lwmsg_context_free_graph(context, type, out);
}

MU_TEST(marshal, string_array_null)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* strings = (not set) */
        0x00
    };

    LWMsgTypeSpec* type = string_array_spec;
    LWMsgBuffer buffer;
    string_array_struct strings;
    string_array_struct *out;

    strings.strings = NULL;

    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &strings, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_POINTER, out->strings, NULL);

    lwmsg_context_free_graph(context, type, out);
}


#define TAG_NUMBER 1
#define TAG_STRING 2

typedef union
{
    int number;
    char* string;
} number_string_union;

static LWMsgTypeSpec number_string_spec[] =
{
    LWMSG_UNION_BEGIN(number_string_union),
    LWMSG_MEMBER_INT32(number_string_union, number),
    LWMSG_ATTR_TAG(TAG_NUMBER),
    LWMSG_MEMBER_PSTR(number_string_union, string),
    LWMSG_ATTR_TAG(TAG_STRING),
    LWMSG_UNION_END,
    LWMSG_TYPE_END
};

typedef struct
{
    int tag1;
    number_string_union* u1;
    int tag2;
    number_string_union* u2;
} two_union_struct;

static LWMsgTypeSpec two_union_spec[] =
{
    LWMSG_STRUCT_BEGIN(two_union_struct),
    LWMSG_MEMBER_INT8(two_union_struct, tag1),
    LWMSG_MEMBER_POINTER_BEGIN(two_union_struct, u1),
    LWMSG_TYPESPEC(number_string_spec),
    LWMSG_ATTR_DISCRIM(two_union_struct, tag1),
    LWMSG_POINTER_END,
    LWMSG_MEMBER_INT8(two_union_struct, tag2),
    LWMSG_MEMBER_POINTER_BEGIN(two_union_struct, u2),
    LWMSG_TYPESPEC(number_string_spec),
    LWMSG_ATTR_DISCRIM(two_union_struct, tag2),
    LWMSG_POINTER_END,
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, two_union)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* tag1 = 1 */
        0x01,
        /* u1 = set */
        0xFF,
        /* u1->number = 42 */
        0x00, 0x00, 0x00, 0x2A,
        /* tag2 = 2 */
        0x02,
        /* u2 = set */
        0xFF,
        /* u2->string = set */
        0xFF,
        /* u2->string length is 4 (implicit) */
        0x00, 0x00, 0x00, 0x04,
        /* u2->string pointee */
        'f', 'o', 'o', '\0'
    };

    LWMsgTypeSpec* type = two_union_spec;
    LWMsgBuffer buffer;
    two_union_struct unions;
    two_union_struct* out;
    number_string_union u1, u2;

    u1.number = 42;
    u2.string = (char*) "foo";

    unions.tag1 = TAG_NUMBER;
    unions.u1 = &u1;
    unions.tag2 = TAG_STRING;
    unions.u2 = &u2;
    
    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &unions, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, unions.tag1, out->tag1);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, unions.tag2, out->tag2);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, unions.u1->number, out->u1->number);
    MU_ASSERT_EQUAL(MU_TYPE_STRING, unions.u2->string, out->u2->string);
}

typedef struct
{
    int foo;
    struct inner_struct {
        int foo;
        const char* bar;
    } inner;
    int bar;
} nested_struct_struct;

static LWMsgTypeSpec nested_struct_spec[] =
{
    LWMSG_STRUCT_BEGIN(nested_struct_struct),
    LWMSG_MEMBER_INT32(nested_struct_struct, foo),
    LWMSG_MEMBER_STRUCT_BEGIN(nested_struct_struct, inner),
    LWMSG_MEMBER_INT32(struct inner_struct, foo),
    LWMSG_MEMBER_PSTR(struct inner_struct, bar),
    LWMSG_STRUCT_END,
    LWMSG_MEMBER_INT32(nested_struct_struct, bar),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, nested_struct)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* foo = 42 */
        0x00, 0x00, 0x00, 0x2A,
        /* inner.foo = 24 */
        0x00, 0x00, 0x00, 0x18,
        /* inner.bar = (set) */
        0xFF,
        /* inner.bar length (implicit) */
        0x00, 0x00, 0x00, 0x04,
        /* inner.bar value */
        'b', 'a', 'r', '\0',
        /* bar = -12 */
        0xFF, 0xFF, 0xFF, 0xF4,
    };

    LWMsgTypeSpec* type = nested_struct_spec;
    LWMsgBuffer buffer;
    nested_struct_struct nested;
    nested_struct_struct* out;

    nested.foo = 42;
    nested.inner.foo = 24;
    nested.inner.bar = "bar";
    nested.bar = -12;
    
    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &nested, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, nested.foo, out->foo);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, nested.inner.foo, out->inner.foo);
    MU_ASSERT_EQUAL(MU_TYPE_STRING, nested.inner.bar, out->inner.bar);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, nested.bar, out->bar);
}

typedef struct
{
    int tag1;
    number_string_union u1;
    int tag2;
    number_string_union u2;
} nested_union_struct;

static LWMsgTypeSpec nested_union_spec[] =
{
    LWMSG_STRUCT_BEGIN(nested_union_struct),
    LWMSG_MEMBER_INT8(nested_union_struct, tag1),
    LWMSG_MEMBER_TYPESPEC(nested_union_struct, u1, number_string_spec),
    LWMSG_ATTR_DISCRIM(nested_union_struct, tag1),
    LWMSG_MEMBER_INT8(nested_union_struct, tag2),
    LWMSG_MEMBER_TYPESPEC(nested_union_struct, u2, number_string_spec),
    LWMSG_ATTR_DISCRIM(nested_union_struct, tag2),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, nested_union)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* tag1 = 2 */
        0x02,
        /* u1.string = (set) */
        0xFF,
        /* u1.string length is 4 (implicit) */
        0x00, 0x00, 0x00, 0x04,
        /* u1.string value */
        'f', 'o', 'o', '\0',
        /* tag2 = 1 */
        0x01,
        /* u2.number = 42 */
        0x00, 0x00, 0x00, 0x2A
    };

    LWMsgTypeSpec* type = nested_union_spec;
    LWMsgBuffer buffer;
    nested_union_struct unions;
    nested_union_struct* out;

    unions.tag1 = TAG_STRING;
    unions.u1.string = (char*) "foo";
    unions.tag2 = TAG_NUMBER;
    unions.u2.number = 42;
    
    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &unions, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, unions.tag1, out->tag1);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, unions.tag2, out->tag2);
    MU_ASSERT_EQUAL(MU_TYPE_STRING, unions.u1.string, out->u1.string);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, unions.u2.number, out->u2.number);
}

typedef struct
{
    int foo;
    int array[2];
    int bar;
} inline_array_struct;

static LWMsgTypeSpec inline_array_spec[] =
{
    LWMSG_STRUCT_BEGIN(inline_array_struct),
    LWMSG_MEMBER_INT16(inline_array_struct, foo),
    LWMSG_MEMBER_ARRAY_BEGIN(inline_array_struct, array),
    LWMSG_INT16(int),
    LWMSG_ARRAY_END,
    LWMSG_ATTR_LENGTH_STATIC(2),
    LWMSG_MEMBER_INT16(inline_array_struct, bar),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, inline_array)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* foo = 1 */
        0x00, 0x01,
        /* array[0] = 2 */
        0x00, 0x02,
        /* array[1] = 3 */
        0x00, 0x03,
        /* bar = 4 */
        0x00, 0x04
    };

    LWMsgTypeSpec* type = inline_array_spec;
    LWMsgBuffer buffer;
    inline_array_struct in;
    inline_array_struct* out;

    in.foo = 1;
    in.array[0] = 2;
    in.array[1] = 3;
    in.bar = 4;
    
    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &in, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.foo, out->foo);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.array[0], out->array[0]);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.array[1], out->array[1]);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.bar, out->bar);
}

typedef struct
{
    int foo;
    char string[];
} flexible_string_struct;

static LWMsgTypeSpec flexible_string_spec[] =
{
    LWMSG_STRUCT_BEGIN(flexible_string_struct),
    LWMSG_MEMBER_INT16(flexible_string_struct, foo),
    LWMSG_MEMBER_ARRAY_BEGIN(flexible_string_struct, string),
    LWMSG_UINT8(char),
    LWMSG_ARRAY_END,
    LWMSG_ATTR_STRING,
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, flexible_string)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* foo = 42 */
        0x00, 0x2A,
        /* string length = 4 (implicit) */
        0x00, 0x00, 0x00, 0x04,
        /* string value */
        'f', 'o', 'o', '\0'
    };

    LWMsgTypeSpec* type = flexible_string_spec;
    LWMsgBuffer buffer;
    flexible_string_struct* in;
    flexible_string_struct* out;

    in = malloc(sizeof(*in) + 4);
    in->foo = 42;
    strcpy(in->string, "foo");

    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, in, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in->foo, out->foo);
    MU_ASSERT_EQUAL(MU_TYPE_STRING, in->string, out->string);

    free(in);
}

typedef struct
{
    int len;
    int array[];
} flexible_array_struct;

static LWMsgTypeSpec flexible_array_spec[] =
{
    LWMSG_STRUCT_BEGIN(flexible_array_struct),
    LWMSG_MEMBER_INT16(flexible_array_struct, len),
    LWMSG_MEMBER_ARRAY_BEGIN(flexible_array_struct, array),
    LWMSG_INT16(int),
    LWMSG_ARRAY_END,
    LWMSG_ATTR_LENGTH_MEMBER(flexible_array_struct, len),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, flexible_array)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set value */
        0xFF,
        /* len = 2 */
        0x00, 0x02,
        /* array[0] = 1 */
        0x00, 0x01,
        /* array[1] = 2 */
        0x00, 0x02
    };

    LWMsgTypeSpec* type = flexible_array_spec;
    LWMsgBuffer buffer;
    flexible_array_struct* in;
    flexible_array_struct* out;

    in = malloc(sizeof(*in) + sizeof(int) * 2);
    in->len = 2;
    in->array[0] = 1;
    in->array[1] = 2;

    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, in, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in->len, out->len);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in->array[0], out->array[0]);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in->array[1], out->array[1]);

    free(in);
}

typedef struct info_level_struct
{
    unsigned int length;
    int level;
    union level_union
    {
        struct level_1_struct
        {
            short number1;
        } *level_1;
        struct level_2_struct
        {
            short number1;
            short number2;
        } *level_2;
    } array;
} info_level_struct;

static LWMsgTypeSpec info_level_spec[] =
{
    LWMSG_STRUCT_BEGIN(struct info_level_struct),
    LWMSG_MEMBER_UINT32(struct info_level_struct, length),
    LWMSG_MEMBER_INT8(struct info_level_struct, level),
    LWMSG_MEMBER_UNION_BEGIN(struct info_level_struct, array),
    LWMSG_MEMBER_POINTER_BEGIN(union level_union, level_1),
    LWMSG_STRUCT_BEGIN(struct level_1_struct),
    LWMSG_MEMBER_INT16(struct level_1_struct, number1),
    LWMSG_STRUCT_END,
    LWMSG_POINTER_END,
    LWMSG_ATTR_LENGTH_MEMBER(struct info_level_struct, length),
    LWMSG_ATTR_TAG(1),
    LWMSG_MEMBER_POINTER_BEGIN(union level_union, level_2),
    LWMSG_STRUCT_BEGIN(struct level_2_struct),
    LWMSG_MEMBER_INT16(struct level_2_struct, number1),
    LWMSG_MEMBER_INT16(struct level_2_struct, number2),
    LWMSG_STRUCT_END,
    LWMSG_POINTER_END,
    LWMSG_ATTR_LENGTH_MEMBER(struct info_level_struct, length),
    LWMSG_ATTR_TAG(2),
    LWMSG_UNION_END,
    LWMSG_ATTR_DISCRIM(struct info_level_struct, level),
    LWMSG_STRUCT_END,
    LWMSG_TYPE_END
};

MU_TEST(marshal, info_level_1)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set */
        0xFF,
        /* length */
        0x00, 0x00, 0x00, 0x02,
        /* level */
        0x01,
        /* array.level_1 set */
        0xFF,
        /* array.level_1[0].number1 */
        0x00, 0x2A,
        /* array.level_1[1].number1 */
        0x00, 0x54
    };

    static struct level_1_struct array[] =
    {
        {42},
        {84}
    };

    LWMsgTypeSpec* type = info_level_spec;
    LWMsgBuffer buffer;
    info_level_struct in;
    info_level_struct* out;
    int i;

    in.length = sizeof(array) / sizeof(*array);
    in.level = 1;
    in.array.level_1 = array;

    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &in, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.length, out->length);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.level, out->level);

    for (i = 0; i < out->length; i++)
    {
        MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.array.level_1[0].number1, out->array.level_1[0].number1);
    }
}

MU_TEST(marshal, info_level_2)
{
    static const unsigned char expected[] =
    {
        /* Implicit pointer set */
        0xFF,
        /* length */
        0x00, 0x00, 0x00, 0x02,
        /* level */
        0x02,
        /* array.level_2 set */
        0xFF,
        /* array.level_2[0].number1 */
        0x00, 0x2A,
        /* array.level_2[0].number2 */
        0xFF, 0xD6,
        /* array.level_2[1].number1 */
        0x00, 0x54,
        /* array.level_2[1].number2 */
        0xFF, 0xAC
    };

    static struct level_2_struct array[] =
    {
        {42, -42},
        {84, -84}
    };

    LWMsgTypeSpec* type = info_level_spec;
    LWMsgBuffer buffer;
    info_level_struct in;
    info_level_struct* out;
    int i;

    in.length = sizeof(array) / sizeof(*array);
    in.level = 2;
    in.array.level_2 = array;

    allocate_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_marshal(context, type, &in, &buffer));

    MU_ASSERT(!memcmp(buffer.memory, expected, sizeof(expected)));

    rewind_buffer(&buffer);

    MU_TRY_CONTEXT(context, lwmsg_unmarshal(context, type, &buffer, (void**) (void*) &out));

    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.length, out->length);
    MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.level, out->level);

    for (i = 0; i < out->length; i++)
    {
        MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.array.level_2[0].number1, out->array.level_2[0].number1);
        MU_ASSERT_EQUAL(MU_TYPE_INTEGER, in.array.level_2[0].number2, out->array.level_2[0].number2);
    }
}
