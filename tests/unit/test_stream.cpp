/**
 * @file tests/unit/test_stream.cpp
 * @brief Test src/stream.*
 */

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#include <src/stream.h>
#include <src/utility.h>

namespace stream {
  std::vector<uint8_t> concat_and_insert(uint64_t insert_size, uint64_t slice_size, const std::string_view &data1, const std::string_view &data2);
}

#include "../tests_common.h"

TEST(ConcatAndInsertTests, ConcatNoInsertionTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(0, 2, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {'a', 'b', 'c', 'd', 'e'};
  ASSERT_EQ(res, expected);
}

TEST(ConcatAndInsertTests, ConcatLargeStrideTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(1, sizeof(b1) + sizeof(b2) + 1, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {0, 'a', 'b', 'c', 'd', 'e'};
  ASSERT_EQ(res, expected);
}

TEST(ConcatAndInsertTests, ConcatSmallStrideTest) {
  char b1[] = {'a', 'b'};
  char b2[] = {'c', 'd', 'e'};
  auto res = stream::concat_and_insert(1, 1, std::string_view {b1, sizeof(b1)}, std::string_view {b2, sizeof(b2)});
  auto expected = std::vector<uint8_t> {0, 'a', 0, 'b', 0, 'c', 0, 'd', 0, 'e'};
  ASSERT_EQ(res, expected);
}

TEST(IndexedIdrTests, AcceptsOnlyExactKnownStreamPayloads) {
  const std::array<char, 2> primary {0, 0};
  const std::array<char, 2> secondary {1, 0};
  const std::array<char, 2> unknown {2, 0};
  const std::array<char, 2> reserved {0, 1};

  EXPECT_EQ(stream::idr_stream_index({primary.data(), primary.size()}), 0);
  EXPECT_EQ(stream::idr_stream_index({secondary.data(), secondary.size()}), 1);
  EXPECT_FALSE(stream::idr_stream_index({unknown.data(), unknown.size()}));
  EXPECT_FALSE(stream::idr_stream_index({reserved.data(), reserved.size()}));
  EXPECT_FALSE(stream::idr_stream_index({}));
  EXPECT_FALSE(stream::idr_stream_index({primary.data(), 1}));
}

TEST(IndexedReferenceFrameInvalidationTests, ParsesPrimaryAndSecondaryRequests) {
  auto make_payload = [](std::int64_t first, std::int64_t last, std::int64_t index) {
    std::array<std::int64_t, 3> words {
      util::endian::little(first),
      util::endian::little(last),
      util::endian::little(index),
    };
    std::string payload(sizeof(words), '\0');
    std::memcpy(payload.data(), words.data(), sizeof(words));
    return payload;
  };

  const auto primary = stream::parse_ref_frame_invalidation(make_payload(12, 34, 0));
  ASSERT_TRUE(primary);
  EXPECT_EQ(primary->first_frame, 12);
  EXPECT_EQ(primary->last_frame, 34);
  EXPECT_EQ(primary->stream_index, 0);

  const auto secondary = stream::parse_ref_frame_invalidation(make_payload(56, 78, 1));
  ASSERT_TRUE(secondary);
  EXPECT_EQ(secondary->first_frame, 56);
  EXPECT_EQ(secondary->last_frame, 78);
  EXPECT_EQ(secondary->stream_index, 1);

  EXPECT_FALSE(stream::parse_ref_frame_invalidation(make_payload(1, 2, -1)));
  EXPECT_FALSE(stream::parse_ref_frame_invalidation(make_payload(1, 2, 2)));
}

TEST(IndexedReferenceFrameInvalidationTests, RejectsEveryNonProtocolSize) {
  for (const auto size : {8U, 16U, 23U, 25U}) {
    EXPECT_FALSE(stream::parse_ref_frame_invalidation(std::string(size, '\0'))) << "size=" << size;
  }
}
