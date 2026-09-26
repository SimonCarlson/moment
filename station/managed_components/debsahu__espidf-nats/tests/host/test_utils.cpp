/**
 * Host-side unit tests for NATSUtil (Array, Queue, RingBuffer, millis, random, secure_zero)
 *
 * Tests the pure-logic components that don't depend on real ESP-IDF hardware.
 * Compile: cd tests/host && make test_utils
 */

// Include the actual library header (fake ESP-IDF headers in fake_esp_idf/)
#include "espidf_nats/util.h"

#include <cassert>
#include <cstdio>
#include <cstring>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %-50s ", name)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("FAIL: %s\n", msg); } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

// ===== Array Tests =====

void test_array_basic() {
    TEST("Array basic push_back and access");
    NATSUtil::Array<int> arr(4);
    size_t idx = arr.push_back(42);
    ASSERT(idx == 0, "first index should be 0");
    ASSERT(arr.size() == 1, "size should be 1");
    ASSERT(arr[0] == 42, "value should be 42");
    PASS();
}

void test_array_growth() {
    TEST("Array auto-growth beyond initial capacity");
    NATSUtil::Array<int> arr(2);
    for (int i = 0; i < 100; i++) {
        arr.push_back(i);
    }
    ASSERT(arr.size() == 100, "size should be 100");
    ASSERT(arr[0] == 0, "first value");
    ASSERT(arr[99] == 99, "last value");
    PASS();
}

void test_array_large() {
    TEST("Array grows to 10000 elements");
    NATSUtil::Array<int> arr(4);
    for (int i = 0; i < 10000; i++) {
        arr.push_back(i * 3);
    }
    ASSERT(arr.size() == 10000, "size should be 10000");
    ASSERT(arr[0] == 0, "first");
    ASSERT(arr[9999] == 29997, "last = 9999*3");
    PASS();
}

void test_array_erase_front() {
    TEST("Array erase first element");
    NATSUtil::Array<int> arr(4);
    arr.push_back(10);
    arr.push_back(20);
    arr.push_back(30);
    arr.erase(0);
    ASSERT(arr.size() == 2, "size after erase");
    ASSERT(arr[0] == 20, "shifted element 0");
    ASSERT(arr[1] == 30, "shifted element 1");
    PASS();
}

void test_array_erase_middle() {
    TEST("Array erase middle element");
    NATSUtil::Array<int> arr(4);
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);
    arr.erase(1);
    ASSERT(arr.size() == 2, "size after erase");
    ASSERT(arr[0] == 1, "first element unchanged");
    ASSERT(arr[1] == 3, "shifted element");
    PASS();
}

void test_array_erase_last() {
    TEST("Array erase last element");
    NATSUtil::Array<int> arr(4);
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);
    arr.erase(2);
    ASSERT(arr.size() == 2, "size after erase");
    ASSERT(arr[0] == 1, "first");
    ASSERT(arr[1] == 2, "second");
    PASS();
}

void test_array_erase_oob() {
    TEST("Array erase out-of-bounds is safe");
    NATSUtil::Array<int> arr(4);
    arr.push_back(1);
    arr.erase(5);  // Should do nothing
    ASSERT(arr.size() == 1, "size unchanged");
    ASSERT(arr[0] == 1, "value unchanged");
    PASS();
}

void test_array_empty_method() {
    TEST("Array clear()/empty() semantics (post-container-safety)");
    NATSUtil::Array<int> arr(4);
    ASSERT(arr.empty(), "empty() true when size==0");
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);
    ASSERT(arr.size() == 3, "size before clear");
    ASSERT(!arr.empty(), "empty() false when populated");
    arr.clear();
    ASSERT(arr.size() == 0, "size after clear should be 0");
    ASSERT(arr.empty(), "empty() true after clear");
    arr.push_back(99);
    ASSERT(arr.size() == 1, "size after re-push");
    ASSERT(arr[0] == 99, "value after re-push");
    PASS();
}

void test_array_pointer_type() {
    TEST("Array with pointer type (Sub*)");
    NATSUtil::Array<void*> arr(4);
    int a = 1, b = 2, c = 3;
    arr.push_back(&a);
    arr.push_back(&b);
    arr.push_back(NULL);
    arr.push_back(&c);
    ASSERT(arr.size() == 4, "size");
    ASSERT(arr[0] == &a, "first pointer");
    ASSERT(arr[2] == NULL, "NULL pointer");
    ASSERT(arr[3] == &c, "fourth pointer");
    // Erase and verify NULLs shift correctly
    arr.erase(1);
    ASSERT(arr.size() == 3, "size after erase");
    ASSERT(arr[0] == &a, "first unchanged");
    ASSERT(arr[1] == NULL, "shifted NULL");
    ASSERT(arr[2] == &c, "shifted last");
    PASS();
}

// ===== Queue Tests =====

void test_queue_basic() {
    TEST("Queue basic FIFO push/pop");
    NATSUtil::Queue<int> q;
    ASSERT(q.empty(), "new queue should be empty");
    ASSERT(q.size() == 0, "size 0");
    q.push(1);
    q.push(2);
    q.push(3);
    ASSERT(q.size() == 3, "size should be 3");
    ASSERT(!q.empty(), "should not be empty");
    ASSERT(q.peek() == 1, "peek should be 1");

    int v1 = q.pop();
    ASSERT(v1 == 1, "first pop should be 1");
    int v2 = q.pop();
    ASSERT(v2 == 2, "second pop should be 2");
    int v3 = q.pop();
    ASSERT(v3 == 3, "third pop should be 3");
    ASSERT(q.empty(), "should be empty now");
    ASSERT(q.size() == 0, "size 0 after draining");
    PASS();
}

void test_queue_pop_empty() {
    TEST("Queue pop on empty returns T()");
    NATSUtil::Queue<int> q;
    int val = q.pop();
    ASSERT(val == 0, "pop on empty int queue returns 0");

    NATSUtil::Queue<void*> pq;
    void* ptr = pq.pop();
    ASSERT(ptr == NULL, "pop on empty ptr queue returns NULL");
    PASS();
}

void test_queue_peek_empty() {
    TEST("Queue peek on empty returns T()");
    NATSUtil::Queue<int> q;
    ASSERT(q.peek() == 0, "peek on empty returns 0");
    PASS();
}

void test_queue_interleaved() {
    TEST("Queue interleaved push/pop");
    NATSUtil::Queue<int> q;
    q.push(1);
    q.push(2);
    ASSERT(q.pop() == 1, "pop 1");
    q.push(3);
    ASSERT(q.pop() == 2, "pop 2");
    ASSERT(q.pop() == 3, "pop 3");
    ASSERT(q.empty(), "empty after draining");
    // Re-use after drain
    q.push(10);
    ASSERT(q.pop() == 10, "pop after refill");
    PASS();
}

void test_queue_large() {
    TEST("Queue 1000 elements FIFO order");
    NATSUtil::Queue<int> q;
    for (int i = 0; i < 1000; i++) {
        q.push(i);
    }
    ASSERT(q.size() == 1000, "size 1000");
    for (int i = 0; i < 1000; i++) {
        int v = q.pop();
        ASSERT(v == i, "FIFO order maintained");
    }
    ASSERT(q.empty(), "empty after 1000 pops");
    PASS();
}

void test_queue_size_t() {
    TEST("Queue<size_t> pop returns 0 on empty (valid SID!)");
    // This demonstrates why the safe pop(T& out) overload is needed
    NATSUtil::Queue<size_t> q;
    size_t val = q.pop();
    ASSERT(val == 0, "pop returns 0 which is a valid SID");
    // A caller that doesn't check empty() first would get SID 0
    PASS();
}

// ===== Millis Timer Tests =====

void test_millis_nonzero() {
    TEST("millis() returns non-zero");
    unsigned long ms = NATSUtil::millis();
    ASSERT(ms > 0, "millis should be > 0");
    PASS();
}

void test_millis_monotonic() {
    TEST("millis() is monotonically increasing");
    unsigned long a = NATSUtil::millis();
    // Busy-wait a tiny bit
    for (volatile int i = 0; i < 100000; i++) {}
    unsigned long b = NATSUtil::millis();
    ASSERT(b >= a, "millis should not decrease");
    PASS();
}

void test_millis_timer() {
    TEST("MillisTimer triggers after interval elapses");
    // MillisTimer triggers when (ms - t) > interval, so interval=0
    // only triggers if at least 1ms has elapsed since creation
    NATSUtil::MillisTimer timer(1);  // 1ms interval
    // Busy-wait to ensure at least 2ms pass
    unsigned long start = NATSUtil::millis();
    while (NATSUtil::millis() - start < 5) {}
    ASSERT(timer.process(), "should trigger after interval elapses");
    PASS();
}

// ===== Random Tests =====

void test_random_range() {
    TEST("random() values in [0, max)");
    for (int i = 0; i < 10000; i++) {
        int r = NATSUtil::random(10);
        ASSERT(r >= 0 && r < 10, "random should be in [0,10)");
    }
    PASS();
}

void test_random_edge_cases() {
    TEST("random() edge cases");
    ASSERT(NATSUtil::random(0) == 0, "random(0) should be 0");
    ASSERT(NATSUtil::random(1) == 0, "random(1) should be 0");
    ASSERT(NATSUtil::random(-5) == 0, "random(negative) should be 0");
    PASS();
}

void test_random_distribution() {
    TEST("random() distribution (not all same value)");
    int counts[10] = {};
    for (int i = 0; i < 10000; i++) {
        counts[NATSUtil::random(10)]++;
    }
    // Each bucket should get roughly 1000. Check none is 0 or > 5000
    bool reasonable = true;
    for (int i = 0; i < 10; i++) {
        if (counts[i] == 0 || counts[i] > 5000) {
            reasonable = false;
            break;
        }
    }
    ASSERT(reasonable, "distribution should be roughly uniform");
    PASS();
}

// ===== Secure Memory Tests =====

void test_secure_zero() {
    TEST("secure_zero clears buffer");
    char buf[32];
    memset(buf, 0xAA, sizeof(buf));
    NATSUtil::secure_zero(buf, sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); i++) {
        ASSERT(buf[i] == 0, "all bytes should be zero");
    }
    PASS();
}

void test_secure_zero_null() {
    TEST("secure_zero(NULL) is safe");
    NATSUtil::secure_zero(NULL, 0);
    NATSUtil::secure_zero(NULL, 100);
    PASS();
}

void test_secure_free() {
    TEST("secure_free frees without crash");
    char* str = strdup("secret password 12345");
    NATSUtil::secure_free(str);
    // Also test NULL
    NATSUtil::secure_free(NULL);
    PASS();
}

// ===== RingBuffer Tests =====

void test_ringbuffer_basic_write_read() {
    TEST("RingBuffer basic write then read");
    NATSUtil::RingBuffer rb(64);
    uint8_t write_data[] = "Hello NATS";
    size_t written = rb.write(write_data, 10);
    ASSERT(written == 10, "should write 10 bytes");
    ASSERT(rb.available() == 10, "10 bytes available");
    ASSERT(!rb.empty(), "not empty");

    uint8_t read_data[16] = {};
    size_t read_count = rb.read(read_data, 10);
    ASSERT(read_count == 10, "should read 10 bytes");
    ASSERT(memcmp(read_data, "Hello NATS", 10) == 0, "data should match");
    ASSERT(rb.empty(), "empty after full read");
    ASSERT(rb.available() == 0, "0 available");
    PASS();
}

void test_ringbuffer_partial_read() {
    TEST("RingBuffer partial read");
    NATSUtil::RingBuffer rb(64);
    uint8_t data[] = "ABCDEFGHIJ";
    rb.write(data, 10);

    uint8_t buf[4] = {};
    size_t n = rb.read(buf, 4);
    ASSERT(n == 4, "read 4 bytes");
    ASSERT(memcmp(buf, "ABCD", 4) == 0, "first 4 bytes");
    ASSERT(rb.available() == 6, "6 remaining");

    n = rb.read(buf, 4);
    ASSERT(n == 4, "read 4 more");
    ASSERT(memcmp(buf, "EFGH", 4) == 0, "next 4 bytes");

    n = rb.read(buf, 4);
    ASSERT(n == 2, "only 2 left");
    ASSERT(memcmp(buf, "IJ", 2) == 0, "last 2 bytes");
    ASSERT(rb.empty(), "empty now");
    PASS();
}

void test_ringbuffer_read_byte() {
    TEST("RingBuffer read_byte one at a time");
    NATSUtil::RingBuffer rb(32);
    uint8_t data[] = "ABC";
    rb.write(data, 3);

    uint8_t b;
    ASSERT(rb.read_byte(&b) == 1, "read 1 byte");
    ASSERT(b == 'A', "first byte A");
    ASSERT(rb.read_byte(&b) == 1, "read 1 byte");
    ASSERT(b == 'B', "second byte B");
    ASSERT(rb.read_byte(&b) == 1, "read 1 byte");
    ASSERT(b == 'C', "third byte C");
    ASSERT(rb.read_byte(&b) == 0, "no more bytes");
    PASS();
}

void test_ringbuffer_wrap_around() {
    TEST("RingBuffer wrap-around (write, read, write again)");
    NATSUtil::RingBuffer rb(16);  // Small buffer to force wrap

    // Fill partially
    uint8_t data1[] = "1234567890";
    rb.write(data1, 10);

    // Read some
    uint8_t buf[8] = {};
    rb.read(buf, 8);
    ASSERT(memcmp(buf, "12345678", 8) == 0, "first read");

    // Write more (will wrap around)
    uint8_t data2[] = "ABCDEF";
    rb.write(data2, 6);

    // Read all remaining: "90" + "ABCDEF"
    uint8_t result[16] = {};
    size_t total = rb.read(result, 16);
    ASSERT(total == 8, "8 bytes remaining");
    ASSERT(memcmp(result, "90ABCDEF", 8) == 0, "wrap-around data correct");
    PASS();
}

void test_ringbuffer_overflow_drops_oldest() {
    TEST("RingBuffer overflow drops oldest data");
    NATSUtil::RingBuffer rb(8);  // capacity 8, usable 7 (one slot wasted)
    uint8_t data[16];
    memset(data, 'X', sizeof(data));
    // Write more than capacity
    size_t written = rb.write(data, 16);
    ASSERT(written == 16, "all 16 bytes 'accepted' (oldest dropped)");
    // Read what's available
    uint8_t out[16] = {};
    size_t avail = rb.available();
    ASSERT(avail > 0 && avail <= 7, "some data available (up to cap-1)");
    rb.read(out, avail);
    // All should be 'X' (same data)
    for (size_t i = 0; i < avail; i++) {
        ASSERT(out[i] == 'X', "data should be X");
    }
    PASS();
}

void test_ringbuffer_clear() {
    TEST("RingBuffer clear resets state");
    NATSUtil::RingBuffer rb(32);
    uint8_t data[] = "test data";
    rb.write(data, 9);
    ASSERT(!rb.empty(), "not empty before clear");
    rb.clear();
    ASSERT(rb.empty(), "empty after clear");
    ASSERT(rb.available() == 0, "0 available after clear");
    // Usable after clear
    rb.write(data, 4);
    ASSERT(rb.available() == 4, "4 available after re-write");
    PASS();
}

void test_ringbuffer_capacity() {
    TEST("RingBuffer get_capacity");
    NATSUtil::RingBuffer rb(256);
    ASSERT(rb.get_capacity() == 256, "capacity should be 256");
    PASS();
}

void test_ringbuffer_null_safety() {
    TEST("RingBuffer NULL parameter safety");
    NATSUtil::RingBuffer rb(32);
    ASSERT(rb.write(NULL, 10) == 0, "write NULL returns 0");
    ASSERT(rb.write((uint8_t*)"x", 0) == 0, "write 0 len returns 0");
    ASSERT(rb.read(NULL, 10) == 0, "read NULL returns 0");
    uint8_t buf[4];
    ASSERT(rb.read(buf, 0) == 0, "read 0 len returns 0");
    PASS();
}

void test_ringbuffer_empty_read() {
    TEST("RingBuffer read on empty returns 0");
    NATSUtil::RingBuffer rb(32);
    uint8_t buf[8];
    ASSERT(rb.read(buf, 8) == 0, "read on empty returns 0");
    ASSERT(rb.read_byte(buf) == 0, "read_byte on empty returns 0");
    PASS();
}

// ===== Main =====

int main() {
    srand(42);  // Seed for reproducibility

    printf("=== NATSUtil Host Tests ===\n\n");

    printf("--- Array ---\n");
    test_array_basic();
    test_array_growth();
    test_array_large();
    test_array_erase_front();
    test_array_erase_middle();
    test_array_erase_last();
    test_array_erase_oob();
    test_array_empty_method();
    test_array_pointer_type();

    printf("\n--- Queue ---\n");
    test_queue_basic();
    test_queue_pop_empty();
    test_queue_peek_empty();
    test_queue_interleaved();
    test_queue_large();
    test_queue_size_t();

    printf("\n--- MillisTimer ---\n");
    test_millis_nonzero();
    test_millis_monotonic();
    test_millis_timer();

    printf("\n--- Random ---\n");
    test_random_range();
    test_random_edge_cases();
    test_random_distribution();

    printf("\n--- Secure Memory ---\n");
    test_secure_zero();
    test_secure_zero_null();
    test_secure_free();

    printf("\n--- RingBuffer ---\n");
    test_ringbuffer_basic_write_read();
    test_ringbuffer_partial_read();
    test_ringbuffer_read_byte();
    test_ringbuffer_wrap_around();
    test_ringbuffer_overflow_drops_oldest();
    test_ringbuffer_clear();
    test_ringbuffer_capacity();
    test_ringbuffer_null_safety();
    test_ringbuffer_empty_read();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    return tests_failed > 0 ? 1 : 0;
}
