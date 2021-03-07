/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2021 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// NOTE(dkorolev): I broke down the `storage` tests into separate source files,
// so that Travis doesn't stall on their compilation for 10+ minutes and call it a failure.
// This is also why this source file is not called `test.cc`, but `storage_run_all_tests.cc`.

// Uncomment the next line for faster Storage REST development iterations. DIMA_FIXME: Remove it.
// #define STORAGE_ONLY_RUN_RESTFUL_TESTS

#include "../tests_smoke/test.cc"
#include "../tests_rest/test.cc"
#include "../tests_flip/test.cc"
