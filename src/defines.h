#pragma once

#define DEFINE_NUMERIC_TYPE(bits, c_type)                                                          \
    using u##bits = unsigned c_type;                                                               \
    using i##bits = signed c_type;                                                                 \
    static_assert(sizeof(u##bits) == bits / 8, "u" #bits " must be " #bits "bit");                 \
    static_assert(sizeof(i##bits) == bits / 8, "i" #bits " must be " #bits "bit");

DEFINE_NUMERIC_TYPE(8, char)
DEFINE_NUMERIC_TYPE(16, short)
DEFINE_NUMERIC_TYPE(32, int)
DEFINE_NUMERIC_TYPE(64, long long);

using f32 = float;
using f64 = double;
static_assert(sizeof(f32) == 4, "f32 must be 32 bit");
static_assert(sizeof(f64) == 8, "f64 must be 64 bit");

using usize = unsigned long long int;
using isize = long long int;

static_assert(sizeof(usize) == sizeof(u8 *), "usize must be system pointer size");
static_assert(sizeof(isize) == sizeof(i8 *), "isize must be system pointer size");
