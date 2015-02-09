/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// Cross-platform portability header.
//
// Ensures that one and only one of BRICKS_{POSIX,APPLE,ANDROID} is defined.
// Keeps the one provided externally. Defaults to environmental setting if none has been defined.

#ifndef BRICKS_PORT_H
#define BRICKS_PORT_H

#include <string>

#ifdef BRICKS_PORT_COUNT
#error "`BRICKS_PORT_COUNT` should not be defined for port.h"
#endif

#define BRICKS_PORT_COUNT (defined(BRICKS_POSIX) + defined(BRICKS_APPLE) + defined(BRICKS_JAVA) + defined(BRICKS_WINDOWS))

#if defined(ANDROID)

#if BRICKS_PORT_COUNT != 0
#error "Should not define any `BRICKS_*` macros when buliding for Android."
#else
#define BRICKS_ANDROID
#endif

#else  // !defined(ANDROID)

#if BRICKS_PORT_COUNT > 1
#error "More than one `BRICKS_*` architectures have been defined."
#elif BRICKS_PORT_COUNT == 0

#if defined(__linux)
#define BRICKS_POSIX
#elif defined(__APPLE__)
#define BRICKS_APPLE
#elif defined(_WIN32)
#define BRICKS_WINDOWS
#else
#error "Could not detect architecture. Please define one of the `BRICKS_*` macros explicitly."
#endif

#endif  // `BRICKS_PORT_COUNT == 0`

#endif  // #elif of `defined(ANDROID)`

#undef BRICKS_PORT_COUNT

#if defined(BRICKS_POSIX)
#define BRICKS_ARCH_UNAME std::string("Linux")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Linux
#elif defined(BRICKS_APPLE)
#define BRICKS_ARCH_UNAME std::string("Darwin")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Darwin
#elif defined(BRICKS_JAVA)
#define BRICKS_ARCH_UNAME std::string("Java")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Java
#elif defined(BRICKS_ANDROID)
#define BRICKS_ARCH_UNAME std::string("Android")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Android
#elif defined(BRICKS_WINDOWS)
#define BRICKS_ARCH_UNAME std::string("Windows")
#define BRICKS_ARCH_UNAME_AS_IDENTIFIER Windows
#else
#error "Unknown architecture."
#endif

#endif
