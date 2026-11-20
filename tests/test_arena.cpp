#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/arena.hpp"
#include "core/schema.hpp"

using opencode::Arena;

TEST(Arena, EmptyInitially) {
  Arena arena;
  EXPECT_EQ(arena.blocks(), 0u);
  EXPECT_EQ(arena.bytes_used(), 0u);
}

TEST(Arena, SingleAllocationFitsFirstBlock) {
  Arena arena(1024);
  auto* p = arena.resource()->allocate(64, alignof(std::max_align_t));
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(arena.blocks(), 1u);
  EXPECT_EQ(arena.bytes_used(), 64u);
  arena.resource()->deallocate(p, 64, alignof(std::max_align_t));
}

TEST(Arena, DeallocationDoesNotReduceUsage) {
  Arena arena(1024);
  auto* p = arena.resource()->allocate(128, 8);
  arena.resource()->deallocate(p, 128, 8);
  EXPECT_EQ(arena.bytes_used(), 128u);
}

TEST(Arena, GrowsWhenRequestExceedsBlock) {
  Arena arena(16);
  auto* p = arena.resource()->allocate(256, 8);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(arena.blocks(), 1u);
  EXPECT_EQ(arena.bytes_used(), 256u);
  arena.resource()->deallocate(p, 256, 8);
}

TEST(Arena, KeepsEarlierAllocationsAfterGrowth) {
  Arena arena(16);
  auto* first = static_cast<std::uint64_t*>(arena.resource()->allocate(8, 8));
  *first = 0xabc;
  auto* second = arena.resource()->allocate(256, 8);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(*first, 0xabc);
  EXPECT_EQ(arena.blocks(), 2u);
}

TEST(Arena, VectorsShareOneResource) {
  Arena arena(64);
  std::pmr::vector<int> nums(arena.resource());
  nums.push_back(1);
  nums.push_back(2);
  std::pmr::string text(arena.resource());
  text = "context";
  EXPECT_EQ(nums.size(), 2u);
  EXPECT_EQ(text, "context");
  EXPECT_EQ(arena.blocks(), 1u);
  EXPECT_GT(arena.bytes_used(), 0u);
}

TEST(Arena, HonorsOverAlignedRequest) {
  Arena arena(32);
  for (int i = 0; i < 8; ++i) {
    auto* p = arena.resource()->allocate(8, 64);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % 64, 0u);
  }
}

TEST(Arena, ZeroByteAllocationIsValid) {
  Arena arena(8);
  auto* p = arena.resource()->allocate(0, 8);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(arena.blocks(), 1u);
  EXPECT_EQ(arena.bytes_used(), 1u);
}

TEST(Arena, ResourcesCompareByIdentity) {
  Arena a;
  Arena b;
  EXPECT_TRUE(a.resource()->is_equal(*a.resource()));
  EXPECT_FALSE(a.resource()->is_equal(*b.resource()));
}

TEST(Arena, RejectsZeroBlockSize) {
  EXPECT_THROW(Arena(0), std::invalid_argument);
}

TEST(Arena, OverAlignedExactSizeAllocationTerminates) {
  Arena arena(8);
  auto* p = arena.resource()->allocate(8, 64);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % 64, 0u);
  EXPECT_EQ(arena.blocks(), 1u);
}

TEST(Arena, RejectsBlockSizeOverflow) {
  Arena arena(8);
  auto huge = [&] {
    (void)arena.resource()->allocate(std::numeric_limits<std::size_t>::max() - 8, 64);
  };
  EXPECT_THROW(huge(), std::overflow_error);
}

TEST(Arena, BytesUsedCountsAllBlocks) {
  Arena arena(16);
  auto* first = arena.resource()->allocate(8, 8);
  ASSERT_NE(first, nullptr);
  auto* second = arena.resource()->allocate(256, 8);
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(arena.bytes_used(), 264u);
}

TEST(Schema, SessionKeepsMessagesInOrder) {
  opencode::Session session;
  session.id = "s1";
  session.agent = "build";
  session.messages = {{"m1", 1, "user", "primeiro"},
                      {"m2", 2, "assistant", "segundo"}};
  ASSERT_EQ(session.messages.size(), 2u);
  EXPECT_EQ(session.messages[0].seq, 1u);
  EXPECT_EQ(session.messages[1].text, "segundo");
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
