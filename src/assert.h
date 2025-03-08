#pragma once

#include <iostream>

#ifdef _MSC_VER
#include <intrin.h>
#define debug_break() __debugbreak()
#else
#define debug_break() __builtin_trap()
#endif

#define always_assert(assertion, error_message)                                                    \
    if (!(assertion)) [[unlikely]] {                                                               \
        std::cerr << __FILE__ << ":" << __LINE__ << " Assertion failure: " << error_message        \
                  << "\nIn function: " << __func__ << std::endl;                                   \
        debug_break();                                                                             \
    }

#ifdef BUILD_RELEASE
#define debug_assert(assertion, error_message)
#else
#define debug_assert(assertion, error_message)                                                     \
    if (!(assertion)) [[unlikely]] {                                                               \
        std::cerr << __FILE__ << ":" << __LINE__ << " Assertion failure: " << error_message        \
                  << "\nIn function: " << __func__ << std::endl;                                   \
        debug_break();                                                                             \
    }

#endif
