#!/bin/bash
#
# This script illustrates how to use the CMakeLists.txt-based build of project that use `current`.
# It is meant to be run from within an empty directory.
#
# The `current/` and `googletest/` directories should not be found in `./`, `../`, and `../../`
# from the directory in which this test script is run.

set -e

# A few precondititons to run this clean test.
if [ -d ./current ] || [ -d ../current ] || [ -d ../../current ] ; then
  echo 'Should not have `current` in `./`, `../, and `../../`.'
  exit 1
fi
if [ -d ./googletest ] || [ -d ../googletest ] || [ -d ../../googletest ] ; then
  echo 'Should not have `googletest` in `./`, `../, and `../../`.'
  exit 1
fi

touch .gitignore
touch golden_gitignore

if ! diff golden_gitignore .gitignore ; then
  echo "Diff failed on line ${LINENO}."
  exit 1
fi

if ! [ -s Makefile ] ; then
  echo 'Need the `Makefile`, `curl`-ing one.'
  curl -s https://raw.githubusercontent.com/C5T/Current/stable/cmake/Makefile >Makefile
fi

# NOTE(dkorolev): This test run `curl`-s the `Makefile`.
# No need to `curl` the very `CMakeLists.txt` file, as the `Makefile` will do it if and as needed.

mkdir -p src
cat >src/hw.cc <<EOF
#include <iostream>
int main() {
#ifdef NDEBUG
  std::cout << "Hello, World! NDEBUG=1." << std::endl;
#else
  std::cout << "Hello, World! NDEBUG is unset." << std::endl;
#endif
}
EOF

# This runs `cmake .` for Release mode, which is output into `.current`.
echo "::group::configure"
make .current
echo "::endgroup::"

# The run of `cmake .` must have cloned `current` and `googletest` and added them into `.gitignore`.
if ! [ -d current ] || ! [ -d googletest ] ; then
  echo 'Either `current` or `googletest` were not cloned.'
  exit 1
fi

echo 'current/' >>golden_gitignore
echo 'googletest/' >>golden_gitignore
if ! diff golden_gitignore .gitignore ; then
  echo "Diff failed on line ${LINENO}."
  exit 1
fi

echo "::group::make [release]"
make
echo "::endgroup::"

echo '.current/' >>golden_gitignore
if ! diff golden_gitignore .gitignore ; then
  echo "Diff failed on line ${LINENO}."
  exit 1
fi

echo "::group::.current/hw"
.current/hw
echo "::endgroup::"

echo "::group::make debug"
make debug
echo "::endgroup::"

echo '.current_debug/' >>golden_gitignore
if ! diff golden_gitignore .gitignore ; then
  echo "Diff failed on line ${LINENO}."
  exit 1
fi

echo "::group::.current_debug/hw"
.current_debug/hw
echo "::endgroup::"

echo

# Test that the `current_build_info.h` file is [re-]generated automatically for each build.
echo "::group::build build_info"
cat >src/build_info.cc <<EOF
#include <iostream>
#ifndef C5T_CMAKE_PROJECT
#error "'C5T_CMAKE_PROJECT' is not defined, are you using Current under 'cmake' with the proper 'CMakeLists.txt'?"
#endif
#include "current_build_info.h"
int main() {
  std::cout << "Successfully built at: " << current::build::cmake::kBuildDateTime << std::endl;
  std::cout << "Autogen .h 'date +%s': " << current::build::cmake::kCurrentBuildHeaderUnixEpochSeconds << std::endl;
}
EOF
make
echo "::endgroup::"

echo

echo "::group::run build_info"
.current/build_info
echo "::endgroup::"

echo

echo "::group::re-build build_info after sleep 1"
sleep 1
touch src/build_info.cc
make
echo "::endgroup::"

echo "::group::re-run build_info"
.current/build_info
echo "::endgroup::"

# This runs `cmake .` for Release mode, which is output into `.current`.
echo "::group::configure"
make .current
echo "::endgroup::"

cat >src/lib_add.cc <<EOF
#include "lib_add.h"
int lib_add(int a, int b) {
  return a + b;
}
EOF

cat >src/lib_add.h <<EOF
#pragma once
int lib_add(int a, int b);
EOF

cat >src/test_gtest.cc <<EOF
#include <gtest/gtest.h>  // IWYU pragma: keep
#include "lib_add.h"
TEST(SmokeGoogletest, TwoTimesTwo) {
  EXPECT_EQ(4, 2 * 2);
}
TEST(SmokeGoogletest, TwoPlusThree) {
  EXPECT_EQ(5, lib_add(2, 3));
}
EOF

cat >src/test_current_gtest.cc <<EOF
#include "3rdparty/gtest/gtest-main.h"  // IWYU pragma: keep
#include "lib_add.h"
TEST(SmokeCurrentGoogletest, TwoTimesTwo) {
  EXPECT_EQ(4, 2 * 2);
}
TEST(SmokeCurrentGoogletest, TwoPlusThree) {
  EXPECT_EQ(5, lib_add(2, 3));
}
EOF

# Force re-configure after adding more into `src/`, appears to be important on the `macos-latest` GitHub test runner.
touch CMakeLists.txt

echo

echo "::group::release_test"
make release_test
echo "::endgroup::"

echo "::group::debug_test"
make debug_test
echo "::endgroup::"

echo

touch src/test_gtest.cc
T0_GTEST=$(date +%s)
echo "::group::one line change google gtest release"
make release_test
echo "::endgroup::"
T1_GTEST=$(date +%s)

touch src/test_current_gtest.cc
T0_CURRENT_GTEST=$(date +%s)
echo "::group::one line change current gtest release"
make release_test
echo "::endgroup::"
T1_CURRENT_GTEST=$(date +%s)

touch src/test_gtest.cc
T0_DEBUG_GTEST=$(date +%s)
echo "::group::one line change google gtest debug"
make debug_test
echo "::endgroup::"
T1_DEBUG_GTEST=$(date +%s)

touch src/test_current_gtest.cc
T0_DEBUG_CURRENT_GTEST=$(date +%s)
echo "::group::one line change current gtest debug"
make debug_test
echo "::endgroup::"
T1_DEBUG_CURRENT_GTEST=$(date +%s)

echo
echo "One-line change time, Current gtest, debug: $((T1_DEBUG_CURRENT_GTEST - T0_DEBUG_CURRENT_GTEST))s"
echo "One-line change time, Current gtest, release: $((T1_CURRENT_GTEST - T0_CURRENT_GTEST))s"
echo
echo "One-line change time, Google gtest, debug: $((T1_DEBUG_GTEST - T0_DEBUG_GTEST))s"
echo "One-line change time, Google gtest, release: $((T1_GTEST - T0_GTEST))s"
echo
echo '(The numbers for `Current gtest` should be worse, as Current is header-only.)'
echo

for i in ./.current/test_gtest ./.current/test_current_gtest ./.current_debug/test_gtest ./.current_debug/test_current_gtest ; do
  echo "::group::test output for $i"
  $i
  echo "::endgroup::"
done

echo

cat >src/dlib_mul.cc <<EOF
extern "C" int dynamic_mul(int a, int b) {
  return a * b;
}
EOF

cat >src/call_dynamic_mul.cc <<EOF
#include <iostream>
#include "bricks/dflags/dflags.h"
#include "bricks/system/syscalls.h"
DEFINE_string(dlib, "", "The path to the '.so' or '.dylib' library to use, without the extension.");
int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  if (FLAGS_dlib.empty()) {
    std::cout << "Must set '--dlib'." << std::endl;
    return 1;
  }
  auto dl = current::bricks::system::DynamicLibrary::CrossPlatform(FLAGS_dlib);
  auto pf = dl.template Get<int (*)(int, int)>("dynamic_mul");
  if (!pf) {
    std::cout << "Something's wrong, the 'dynamic_mul' function was not found in the library'." << std::endl;
    return 1;
  }
  auto f = *pf;
  int result = f(2, 3);
  if (result == 6) {
    std::cout << "OK, 2 * 3 == 6." << std::endl;
  } else {
    std::cout << "WA, 2 * 3 != " << result << '.' << std::endl;
    return 1;
  }
}
EOF

# This forces `cmake` re-configuration, for both Debug and Release builds.
# TODO(dkorolev): This should probably happen automatically if the set of files under `src/` has changed!
touch CMakeLists.txt

echo "::group::build .current/libdlib_mul.{so|dylib} and .current/call_dynamic_mul"
make
echo "::endgroup::"
echo "::group::build .current_debug/libdlib_mul.{so|dylib} and .current_debug/call_dynamic_mul"
make debug
echo "::endgroup::"

echo "::group::call .so-defined functions from manually built binaries, debug <=> release"
./.current/call_dynamic_mul --dlib .current/libdlib_mul
./.current/call_dynamic_mul --dlib .current_debug/libdlib_mul
./.current_debug/call_dynamic_mul --dlib .current_debug/libdlib_mul
./.current_debug/call_dynamic_mul --dlib .current/libdlib_mul
echo "::endgroup::"
