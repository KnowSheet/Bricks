/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Andrei Drozdov <sulverus@gmail.com>

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
#define CURRENT_MOCK_TIME

#include "../../3rdparty/gtest/gtest-main.h"
#include "../../bricks/time/chrono.h"
#include "vector_clock.h"

using namespace std::chrono_literals;

TEST(VectorClock, SmokeTest) {
  auto v = VectorClock();
  v.Step();

  auto data = DiscreteClocks(1);
  EXPECT_FALSE(v.AdvanceTo(data)) << "Merge from future should return false.";
  EXPECT_EQ(v.State().size(), static_cast<size_t>(1));

  auto v2 = VectorClock(data, 0);
  EXPECT_TRUE(v2.AdvanceTo(data)) << "Test lte t==t'.";
}

TEST(VectorClock, ToString) {
  DiscreteClocks c1 = {1, 2};
  auto v = VectorClock(c1, 0);
  EXPECT_EQ(v.ToString(), "VCLOCK ID=0: [1, 2]");
}

TEST(VectorClock, Merge) {
  auto base_time = current::time::Now();
  DiscreteClocks c1 = {1, 2};
  auto v = VectorClock(c1, 0);

  // Merge correct update
  DiscreteClocks c2 = {2, 3};
  EXPECT_TRUE(v.AdvanceTo(c2)) << "Each element is greater - ok to merge.";
  auto cur_state = v.State();
  EXPECT_GT(cur_state[0], c2[0]) << "Local time should be updated after merge.";
  EXPECT_EQ(cur_state[1], c2[1]) << "Merged time should be equal c2[1].";

  c2 = {1, 2};
  EXPECT_FALSE(v.AdvanceTo(c2)) << "Cant merge T > T' - incorrect update.";
  EXPECT_EQ(v.State()[0], cur_state[0] + 1) << "Invalid data, merged vector.";
  EXPECT_EQ(v.State()[1], cur_state[1]);

  // Merge partially equals using lte validation
  v = VectorClock(c1, 0);
  c2 = {1, 3};
  EXPECT_TRUE(v.AdvanceTo(c2)) << "0 is equals, 1 is greater - ok to merge.";
  cur_state = v.State();
  EXPECT_GT(cur_state[0], c2[0]) << "Local time should be updated after merge.";
  EXPECT_EQ(c2[1], cur_state[1]) << "Merged time should be equal c2[1].";

  v = VectorClock(c1, 0);
  cur_state = v.State();
  c2 = DiscreteClocks({1, 0});
  EXPECT_FALSE(v.AdvanceTo(c2)) << "Merge partially incorrect.";
  EXPECT_EQ(v.State()[0], cur_state[0] + 1) << "Invalid data, merged vector.";
  EXPECT_EQ(v.State()[1], cur_state[1]);
}

TEST(VectorClock, ContinuousTime) {
  current::time::ResetToZero();
  current::time::SetNow(0us, 1000us);
  auto base_time = current::time::Now();
  ContinuousClocks c1 = {base_time, base_time + 100us};
  auto v = VectorClock<ContinuousClocks, MergeStrategy>(c1, 0);

  ContinuousClocks c2 = {base_time + 200us, base_time + 300us};
  EXPECT_TRUE(v.AdvanceTo(c2)) << "Merge correct update. Each elemnt is greater - ok.";
  auto cur_state = v.State();

  c2 = {base_time + 100us, base_time + 200us};
  EXPECT_FALSE(v.AdvanceTo(c2)) << "Can't apply T > T'. Merge incorrect update after previous update.";
}

TEST(VectorClock, StrictMerge) {
  auto base_time = current::time::Now();
  DiscreteClocks c1 = {1, 2};
  auto v = VectorClock<DiscreteClocks, StrictMergeStrategy>(c1, 0);

  DiscreteClocks c2 = {2, 3};
  EXPECT_TRUE(v.AdvanceTo(c2)) << "Each element is greater - ok to merge.";
  auto cur_state = v.State();
  EXPECT_GT(cur_state[0], c2[0]) << "Local time should be updated after merge.";
  EXPECT_EQ(c2[1], cur_state[1]) << "Merged time should be equal c2[1]";

  c1 = {10, 20};
  v = VectorClock<DiscreteClocks, StrictMergeStrategy>(c1, 0);
  cur_state = v.State();
  c2 = {10, 20};
  EXPECT_FALSE(v.AdvanceTo(c2)) << "Merge equals using strict validation. Not ok to apply.";
  EXPECT_EQ(v.State()[0], cur_state[0] + 1);
  EXPECT_EQ(v.State()[1], cur_state[1]);

  v = VectorClock<DiscreteClocks, StrictMergeStrategy>(c1, 0);
  cur_state = v.State();
  c2 = {1, 20};
  EXPECT_FALSE(v.AdvanceTo(c2)) << "Merge partially equals. 0 is equal, 1 is greater - not or to apply.";
  EXPECT_EQ(v.State()[0], cur_state[0] + 1);
  EXPECT_EQ(v.State()[1], cur_state[1]);

  v = VectorClock<DiscreteClocks, StrictMergeStrategy>(c1, 0);
  cur_state = v.State();
  c2 = {0, 1};
  EXPECT_FALSE(v.AdvanceTo(c2)) << "Incorrect update, initial state was no changed.";
  EXPECT_EQ(v.State()[0], cur_state[0] + 1);
  EXPECT_EQ(v.State()[1], cur_state[1]);
}
