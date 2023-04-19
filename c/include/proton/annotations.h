#ifndef PROTON_ANNOTATIONS_H
#define PROTON_ANNOTATIONS_H 1

/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @cond INTERNAL
 *
 * Compiler specific mechanisms for annotating functions and function parameters.
 *
 * SAL (MSVC) documentation:
 *  https://learn.microsoft.com/en-us/cpp/c-runtime-library/sal-annotations?view=msvc-170
 *  https://learn.microsoft.com/en-us/cpp/code-quality/understanding-sal?view=msvc-170&viewFallbackFrom=vs-2019
 *  https://learn.microsoft.com/en-us/cpp/code-quality/annotating-function-parameters-and-return-values?view=msvc-170
 * GCC documentation: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html
 * Clang documentation: https://clang.llvm.org/docs/AttributeReference.html
 * Clang Analyzer documentation: https://clang-analyzer.llvm.org/annotations.html
 *
 * Example usage for the implemented annotation macros:
 *
 * std::string stringPrintf(PN_PRINTF_FORMAT const char* format, ...)
 *   PN_PRINTF_FORMAT_ATTR(1, 2);
 *
 * PN_NODISCARD std::unique_lock<Mutex> make_unique_lock(
 *   Mutex& mutex, Args&&... args) { ... }
 *
 */

// compiler specific attribute translation
// msvc should come first, so if clang is in msvc mode it gets the right defines

// warn format placeholders

// NOTE: this will only do checking in msvc with versions that support /analyze
#ifdef _MSC_VER

    #include <stddef.h>

    #ifdef _USE_ATTRIBUTES_FOR_SAL
        #undef _USE_ATTRIBUTES_FOR_SAL
    #endif

    #define _USE_ATTRIBUTES_FOR_SAL 1
    #include <sal.h>

    #define PN_PRINTF_FORMAT _Printf_format_string_
    #define PN_PRINTF_FORMAT_ATTR(format_param, dots_param) /**/

#elif defined(__GNUC__)

    #define PN_PRINTF_FORMAT /**/
    #define PN_PRINTF_FORMAT_ATTR(format_param, dots_param) \
      __attribute__((__format__(__printf__, format_param, dots_param)))

#else

    #define PN_PRINTF_FORMAT /**/
    #define PN_PRINTF_FORMAT_ATTR(format_param, dots_param) /**/

#endif


// warn unused result
#if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(nodiscard)
        #define PN_NODISCARD [[nodiscard]]
    #endif
#endif
#if !defined PN_NODISCARD
    #if defined(_MSC_VER) && (_MSC_VER >= 1700)
        #define PN_NODISCARD _Check_return_
    #elif defined(__GNUC__)
        #define PN_NODISCARD __attribute__((__warn_unused_result__))
    #else
        #define PN_NODISCARD
    #endif
#endif

#endif /* annotations.h */
