#!/bin/bash
#
# Compiles all the tests into one binary, runs all the tests and generates the complete coverage report.
#
# Currently supports Linux only due to the coverage generation tool used.
#
# TODO(dkorolev): Look into making this script run on Mac.

set -u -e

CPPFLAGS="-std=c++11 -g -Wall -W -fprofile-arcs -ftest-coverage -DCURRENT_COVERAGE_REPORT_MODE"
LDFLAGS="-pthread -ldl"
if [ $(uname) = "Darwin" ] ; then
  CPPFLAGS+=" -stdlib=libc++ -x objective-c++ -fobjc-arc"
  LDFLAGS+=" -framework Foundation"
fi

# Magic to have `make test` work from `Current/`, `Current/Bricks/`, `Current/Blocks/`, etc.
PRE_CURRENT_SCRIPTS_DIR="$(dirname "${BASH_SOURCE[0]}")"
CURRENT_SCRIPTS_DIR="$("$PRE_CURRENT_SCRIPTS_DIR/fullpath.sh" "$PRE_CURRENT_SCRIPTS_DIR")"
RUN_DIR_FULL_PATH="$("$CURRENT_SCRIPTS_DIR/fullpath.sh" "$PWD")"

# NOTE: FULL_TEST_DIR must be resolved from the current working directory.
FULL_TEST_DIR_NAME="zzz_full_test"  # The `zzz` prefix guarantees the full test directory is down the list.
FULL_TEST_DIR_FULL_PATH="$RUN_DIR_FULL_PATH/$FULL_TEST_DIR_NAME"

ALL_TESTS_TOGETHER="everything"

GOLDEN_DIR_NAME="golden"
GOLDEN_FULL_PATH="$FULL_TEST_DIR_FULL_PATH/$GOLDEN_DIR_NAME"

# Concatenate all `test.cc` files in the right way, creating one test to rule them all.
mkdir -p "$FULL_TEST_DIR_NAME"
(
echo '// This file is auto-generated by Current/scripts/full-test.sh.' ;
echo '// It is updated by running this script from the top-level directory,'
echo '// tests underneath which should be build in a bundle and run.'
echo '// For top-level tests, this file is useful to check in, so that non-*nix'
echo '// systems can too compile and run all the tests in a bundle.'
echo
echo '#define CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS'
echo '#define CURRENT_OWNED_BORROWED_EXPLICIT_ONLY_CONSTRUCTION'
echo
echo '#include "port.h"           // To have `std::{min/max}` work in Visual Studio, need port.h before STL headers.'
echo '#include "current_build.h"  // Always use local version generated by `full-test.sh`.'
echo
echo '#include "../Bricks/dflags/dflags.h"'
echo '#include "../3rdparty/gtest/gtest-main-with-dflags.h"'
echo
echo '#define CURRENT_MOCK_TIME'  # Assume and require none of the tests require wall time.
echo
) > "$FULL_TEST_DIR_NAME/$ALL_TESTS_TOGETHER.cc"


echo -n -e "\033[0m\033[1mTests:\033[0m\033[36m"
for i in $(find . -iname "*test.cc" | grep -v "/3rdparty/" | grep -v "/.current/" | grep -v "/sandbox/" | sort -g); do
  echo "#include \"$i\"" >> "$FULL_TEST_DIR_NAME/$ALL_TESTS_TOGETHER.cc"
  echo -n " $i"
done

# Allow this one test to rule them all to access all the `golden/` files.
mkdir -p "$GOLDEN_FULL_PATH"
echo -e "\n\n\033[0m\033[1mGolden files\033[0m: \033[35m"
for dir in $(find . -iname "$GOLDEN_DIR_NAME" -type d | grep -v "/3rdparty/" | grep -v "/.current/" | grep -v "/sandbox/" | grep -v "$FULL_TEST_DIR_NAME"); do
  (cd $dir; for filename in * ; do cp -v "$PWD/$filename" "$GOLDEN_FULL_PATH" ; done)
done
echo -e -n "\033[0m"

(
  # Build and run The Big Test.

  ln -sf "$CURRENT_SCRIPTS_DIR/MakefileWithCurrentBuild" "$FULL_TEST_DIR_NAME/Makefile"
  (cd "$FULL_TEST_DIR_NAME" ; make phony_current_build > /dev/null ; unlink Makefile)

  cd "$FULL_TEST_DIR_NAME"

  echo -e "\033[0m"
  echo -n -e "\033[1mCompiling all tests together: \033[0m\033[31m"
  g++ $CPPFLAGS -I . -I .. "$ALL_TESTS_TOGETHER.cc" -o "$ALL_TESTS_TOGETHER" $LDFLAGS
  echo -e "\033[32m\033[1mOK.\033[0m"

  echo -e "\033[1mRunning the tests and generating coverage info.\033[0m"
  find . -name '*.gcda' -delete
  "./$ALL_TESTS_TOGETHER" || exit 1
  echo -e "\n\033[32m\033[1mALL TESTS PASS.\033[0m"

  # Generate the resulting code coverage report.
  gcov "$ALL_TESTS_TOGETHER.cc" >/dev/null
  geninfo . --output-file coverage0.info >/dev/null
  lcov -r coverage0.info /usr/include/\* \*/gtest/\* \*/3rdparty/\* -o coverage.info >/dev/null
  genhtml coverage.info --output-directory coverage/ >/dev/null
  rm -rf coverage.info coverage0.info *.gcov *.gcda *.gcno
  echo
  echo -e -n "\033[0m\033[1mCoverage report\033[0m: \033[36m"
  echo -n "$FULL_TEST_DIR_FULL_PATH/coverage/index.html"
  echo -e "\033[0m"
)
