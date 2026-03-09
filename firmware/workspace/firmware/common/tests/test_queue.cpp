/*
 * @file test_queue.cpp
 * @brief Unit tests for the queue module using Google Test (BARR-C:2018 compliant)
 */
#include <gtest/gtest.h>

extern "C"
{
    #include "queue.h"
}


#include <stdint.h>
#include <cstring>

#define TEST_QUEUE_SIZE      (8u)
#define TEST_ELEMENT_SIZE    (sizeof(int32_t))

class QueueTest : public ::testing::Test
{
protected:
    queue_t test_queue;
    uint8_t test_storage[TEST_QUEUE_SIZE * TEST_ELEMENT_SIZE];

    virtual void SetUp(void)
    {
        (void)queue_init(&test_queue, test_storage, sizeof(test_storage), TEST_ELEMENT_SIZE);
    }
};

TEST_F(QueueTest, EnqueueAndDequeue)
{
    int32_t value1 = 42;
    int32_t value2 = 24;
    int32_t out1 = 0;
    int32_t out2 = 0;

    // Insert first value and check count
    ASSERT_EQ(RESULT_OK, test_queue.interface.enqueue(&test_queue.interface, &value1));
    ASSERT_EQ(test_queue._element_count, 1u);

    // Insert second value and check count
    ASSERT_EQ(RESULT_OK, test_queue.interface.enqueue(&test_queue.interface, &value2));
    ASSERT_EQ(test_queue._element_count, 2u);   

    // Dequeue first value and check
    ASSERT_EQ(RESULT_OK, test_queue.interface.dequeue(&test_queue.interface, &out1));
    ASSERT_EQ(value1, out1);
    ASSERT_EQ(test_queue._element_count, 1u);

    // Dequeue second value and check       
    ASSERT_EQ(RESULT_OK, test_queue.interface.dequeue(&test_queue.interface, &out2));
    ASSERT_EQ(value2, out2);
}

TEST_F(QueueTest, Peek)
{
    int32_t value = 123;
    int32_t out = 0;

    // Enqueue value and check count    
    ASSERT_EQ(RESULT_OK, test_queue.interface.enqueue(&test_queue.interface, &value));
    ASSERT_EQ(test_queue._element_count, 1u);

    // Peek value and check it matches  
    ASSERT_EQ(RESULT_OK, test_queue.interface.peek(&test_queue.interface, &out));
    ASSERT_EQ(value, out);

    // Ensure count remains the same after peek
    ASSERT_EQ(test_queue._element_count, 1u);
}

TEST_F(QueueTest, PeekMultiple)
{
    int32_t values[3] = {1, 2, 3};
    int32_t out[3] = {0};
    
    // Enqueue multiple values and check count
    for (size_t idx = 0; idx < 3u; idx++)
    {
        (void)test_queue.interface.enqueue(&test_queue.interface, &values[idx]);
    }
    ASSERT_EQ(test_queue._element_count, 3u);

    // Peek multiple values and check they match and that count remains the same
    ASSERT_EQ(RESULT_OK, test_queue.interface.peek_multiple(&test_queue.interface, out, 3u));
    ASSERT_EQ(0, std::memcmp(values, out, sizeof(values)));
    ASSERT_EQ(test_queue._element_count, 3u);
}

TEST_F(QueueTest, DequeueMultiple)
{
    int32_t values[4] = {10, 20, 30, 40};
    int32_t out[4] = {0};

    // Enqueue multiple values and check count
    for (size_t idx = 0; idx < 4u; idx++)
    {
        (void)test_queue.interface.enqueue(&test_queue.interface, &values[idx]);
    }
    ASSERT_EQ(test_queue._element_count, 4u);

    // Dequeue multiple values and check they match
    ASSERT_EQ(RESULT_OK, test_queue.interface.dequeue_multiple(&test_queue.interface, out, 4u));
    ASSERT_EQ(0, std::memcmp(values, out, sizeof(values)));

    // Ensure count is zero after dequeueing all
    ASSERT_EQ(test_queue._element_count, 0u);
}

TEST_F(QueueTest, PopMultiple)
{
    int32_t values[5] = {5, 6, 7, 8, 9};
    int32_t out[2] = {0};

    // Enqueue multiple values and check count
    for (size_t idx = 0; idx < 5u; idx++)
    {
        (void)test_queue.interface.enqueue(&test_queue.interface, &values[idx]);
    }
    ASSERT_EQ(test_queue._element_count, 5u);

    // Pop multiple values and check count
    ASSERT_EQ(RESULT_OK, test_queue.interface.pop_multiple(&test_queue.interface, 3u));
    ASSERT_EQ(test_queue._element_count, 2u);

    // Dequeue remaining values and check they match
    ASSERT_EQ(RESULT_OK, test_queue.interface.dequeue_multiple(&test_queue.interface, out, 2u));
    ASSERT_EQ(0, std::memcmp(&values[3], out, 2 * sizeof(int32_t)));
    ASSERT_EQ(test_queue._element_count, 0u);
}

TEST_F(QueueTest, Flush)
{
    int32_t values[3] = {99, 100, 101};

    // Enqueue multiple values
    for (size_t idx = 0; idx < 3u; idx++)
    {
        (void)test_queue.interface.enqueue(&test_queue.interface, &values[idx]);
    }
    ASSERT_EQ(test_queue._element_count, 3u);

    // Flush the queue
    ASSERT_EQ(RESULT_OK, test_queue.interface.flush(&test_queue.interface));
    ASSERT_EQ(test_queue._element_count, 0u);

    // Attempt to dequeue after flush, should be empty
    int32_t out = 0;
    ASSERT_EQ(QUEUE_EMPTY, GET_ERR_CODE(test_queue.interface.dequeue(&test_queue.interface, &out)));
}

TEST_F(QueueTest, PeekMultipleWrap)
{
    int32_t INITIAL_values[] = {0, 1, 2 ,3, 4, 5, 6, 7};
    static_assert(sizeof(INITIAL_values)/sizeof(INITIAL_values[0]) == TEST_QUEUE_SIZE, "INITIAL_values size must match TEST_QUEUE_SIZE");
    
    int32_t out[TEST_QUEUE_SIZE] = {0};
    
    // Fill the queue completely and check count
    for (size_t idx = 0; idx < TEST_QUEUE_SIZE; idx++)
    {
        (void)test_queue.interface.enqueue(&test_queue.interface, &INITIAL_values[idx]);
    }    
    ASSERT_EQ(test_queue._element_count, TEST_QUEUE_SIZE);
    
    // Remove half the elements to force wrap and check count
    size_t elements_to_remove = TEST_QUEUE_SIZE / 2;  // Define a number here in case TEST_QUEUE_SIZE is uneven
    (void)test_queue.interface.pop_multiple(&test_queue.interface, elements_to_remove);
    ASSERT_EQ(test_queue._element_count, TEST_QUEUE_SIZE - elements_to_remove);    // Should be half full now
    
    // Enqueue more elements to cause wrap around
    int32_t wrap_values[] = {8, 9, 10, 11};
    ASSERT_EQ(sizeof(wrap_values)/sizeof(wrap_values[0]) >= elements_to_remove, true); // Ensure enough wrap values are provided
    for (size_t idx = 0; idx < elements_to_remove; idx++)
    {
        (void)test_queue.interface.enqueue(&test_queue.interface, &wrap_values[idx]);
    }
    ASSERT_EQ(test_queue._element_count, TEST_QUEUE_SIZE);

    // Now peek multiple (should wrap internally)
    int32_t expected_values[TEST_QUEUE_SIZE] = {4, 5, 6, 7, 8, 9, 10, 11};
    ASSERT_EQ(RESULT_OK, test_queue.interface.peek_multiple(&test_queue.interface, out, TEST_QUEUE_SIZE));
    ASSERT_EQ(0, std::memcmp(expected_values, out, sizeof(expected_values)));
}

TEST_F(QueueTest, PopMultipleWrap)
{
    int32_t INITIAL_values[] = {200, 201, 202, 203, 204, 205, 206, 207};
    static_assert(sizeof(INITIAL_values)/sizeof(INITIAL_values[0]) == TEST_QUEUE_SIZE, "INITIAL_values size must match TEST_QUEUE_SIZE");

    for (size_t idx = 0; idx < TEST_QUEUE_SIZE; idx++)
    {
        (void)test_queue.interface.enqueue(&test_queue.interface, &INITIAL_values[idx]);
    }
    ASSERT_EQ(test_queue._element_count, TEST_QUEUE_SIZE);

    // Remove half the elements to force wrap and check count
    size_t elements_to_remove = TEST_QUEUE_SIZE / 2;
    (void)test_queue.interface.pop_multiple(&test_queue.interface, elements_to_remove);
    ASSERT_EQ(test_queue._element_count, TEST_QUEUE_SIZE - elements_to_remove);

    // Enqueue more elements to cause wrap around
    int32_t wrap_values[] = {300, 301, 302, 303};
    ASSERT_EQ(sizeof(wrap_values)/sizeof(wrap_values[0]) >= elements_to_remove, true);
    for (size_t idx = 0; idx < elements_to_remove; idx++)
    {
        (void)test_queue.interface.enqueue(&test_queue.interface, &wrap_values[idx]);
    }
    ASSERT_EQ(test_queue._element_count, TEST_QUEUE_SIZE);

    // Pop all elements (should wrap internally)
    ASSERT_EQ(RESULT_OK, test_queue.interface.pop_multiple(&test_queue.interface, TEST_QUEUE_SIZE));
    ASSERT_EQ(test_queue._element_count, 0u);
}

TEST_F(QueueTest, EnqueueMultiple)
{
    int32_t values[5] = {11, 22, 33, 44, 55};
    int32_t out[5] = {0};
    
    // Enqueue multiple elements using enqueue_multiple
    ASSERT_EQ(RESULT_OK, test_queue.interface.enqueue_multiple(&test_queue.interface, values, 5u));
    ASSERT_EQ(test_queue._element_count, 5u);

    // Dequeue all and check order
    ASSERT_EQ(RESULT_OK, test_queue.interface.dequeue_multiple(&test_queue.interface, out, 5u));
    ASSERT_EQ(0, std::memcmp(values, out, sizeof(values)));
    ASSERT_EQ(test_queue._element_count, 0u);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
