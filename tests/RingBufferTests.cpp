#include "../include/RingBuffer.hpp"

#include <gtest/gtest.h>

template <typename T> class RingBufferTypedTest : public ::testing::Test
{
protected:
    RingBuffer<T> buffer;

    RingBufferTypedTest()
        : buffer(5)
    {
    }

    void SetUp() override
    {
        // Optional: Code here will be called immediately after the constructor (right before each test).
    }

    void TearDown() override
    {
        // Optional: Code here will be called immediately after each test (right before the destructor).
    }
};

using MyTypes = ::testing::Types<int, float, double>;
TYPED_TEST_SUITE(RingBufferTypedTest, MyTypes);

TYPED_TEST(RingBufferTypedTest, PushPopTest)
{
    EXPECT_TRUE(this->buffer.push(1));
    EXPECT_TRUE(this->buffer.push(2));
    EXPECT_TRUE(this->buffer.push(3));

    TypeParam item;
    EXPECT_TRUE(this->buffer.pop(item));
    EXPECT_EQ(item, 1);

    EXPECT_TRUE(this->buffer.pop(item));
    EXPECT_EQ(item, 2);

    EXPECT_TRUE(this->buffer.pop(item));
    EXPECT_EQ(item, 3);

    EXPECT_FALSE(this->buffer.pop(item)); // Buffer should be empty
}

TYPED_TEST(RingBufferTypedTest, BufferSizeTest)
{
    EXPECT_EQ(this->buffer.size(), 0);

    this->buffer.push(1);
    this->buffer.push(2);
    this->buffer.push(3);

    EXPECT_EQ(this->buffer.size(), 3);

    TypeParam item;
    this->buffer.pop(item);
    EXPECT_EQ(this->buffer.size(), 2);

    this->buffer.pop(item);
    EXPECT_EQ(this->buffer.size(), 1);

    this->buffer.pop(item);
    EXPECT_EQ(this->buffer.size(), 0);
}
