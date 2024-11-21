#include <fpe/ring_buffer.hpp>
#include <gtest/gtest.h>

// Helper template to extract type and size from tuple
template <typename T> struct RingBufferTestTraits;

template <typename ValueType, std::size_t Size>
struct RingBufferTestTraits<
    std::tuple<ValueType, std::integral_constant<std::size_t, Size>>> {
  using Type = ValueType;
  static constexpr std::size_t size = Size;
};

// Define the test fixture for RingBuffer with various types and capacities
template <typename T> class RingBufferTypedTest : public ::testing::Test {
protected:
  using ValueType = typename RingBufferTestTraits<T>::Type;
  static constexpr std::size_t Size = RingBufferTestTraits<T>::size;

  fpe::RingBuffer<ValueType, Size> buffer;

  RingBufferTypedTest() : buffer() {}

  void SetUp() override {
    // Optional setup code if needed
  }

  void TearDown() override {
    // Optional teardown code if needed
  }
};

// Define the types and sizes to test with Google Test Types
using TestTypes = ::testing::Types<
    std::tuple<int, std::integral_constant<std::size_t, 5>>,
    std::tuple<float, std::integral_constant<std::size_t, 10>>,
    std::tuple<double, std::integral_constant<std::size_t, 15>>>;

TYPED_TEST_SUITE(RingBufferTypedTest, TestTypes);

// Define each test with TYPED_TEST
TYPED_TEST(RingBufferTypedTest, PushPopTest) {
  using ValueType = typename TestFixture::ValueType;
  auto &buffer = TestFixture::buffer;

  // Test pushing and popping a value
  buffer.push(static_cast<ValueType>(42));
  std::optional<ValueType> value = buffer.pop();
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value, static_cast<ValueType>(42));
}

TYPED_TEST(RingBufferTypedTest, SizeTest) {
  auto &buffer = TestFixture::buffer;
  EXPECT_EQ(buffer.size(), 0); // Buffer initially empty

  // Push items up to capacity
  for (std::size_t i = 0; i < TestFixture::Size; ++i) {
    buffer.push(static_cast<typename TestFixture::ValueType>(i));
  }
  EXPECT_EQ(buffer.size(), TestFixture::Size); // Should match capacity

  // Pop all items and check size returns to 0
  std::optional<typename TestFixture::ValueType> value;
  for (std::size_t i = 0; i < TestFixture::Size; ++i) {
    value = buffer.pop();
  }
  EXPECT_EQ(buffer.size(), 0);
}

TYPED_TEST(RingBufferTypedTest, OverflowTest) {
  using ValueType = typename TestFixture::ValueType;
  auto &buffer = TestFixture::buffer;
  constexpr std::size_t bufferSize = TestFixture::Size;

  // Fill the buffer to test overflow behavior
  for (std::size_t i = 0; i < bufferSize; ++i) {
    buffer.push(static_cast<ValueType>(i));
  }

  // The next push should overwrite the oldest item
  buffer.push(static_cast<ValueType>(99));

  // Pop all items and verify the overwritten behavior
  std::optional<ValueType> value;
  for (std::size_t i = 1; i < bufferSize; ++i) {
    value = buffer.pop();
    EXPECT_TRUE(value.has_value());
    EXPECT_EQ(value, static_cast<ValueType>(i));
  }
  value = buffer.pop();
  EXPECT_TRUE(value.has_value());
  EXPECT_EQ(value, static_cast<ValueType>(99));
}

TYPED_TEST(RingBufferTypedTest, EmptyPopTest) {
  using ValueType = typename TestFixture::ValueType;
  auto &buffer = TestFixture::buffer;

  // Test pop fails on empty buffer
  std::optional<ValueType> value;
  EXPECT_FALSE(value.has_value());
}
