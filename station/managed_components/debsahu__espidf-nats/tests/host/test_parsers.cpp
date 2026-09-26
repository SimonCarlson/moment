/**
 * Host-side unit tests for NATSParsers (typed response parsers)
 *
 * Tests parse_pub_ack, parse_account_info, parse_object_meta, is_error_response
 * Note: parsers.h must exist in include/espidf_nats/ (from fix/typed-parsers branch)
 *
 * Compile: cd tests/host && make test_parsers
 */

// Include the actual library headers (fake ESP-IDF headers in fake_esp_idf/)
#include "espidf_nats/types.h"
#include "espidf_nats/parsers.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) printf("  TEST: %-50s ", name)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("FAIL: %s\n", msg); } while(0)
#define ASSERT(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

// ===== parse_pub_ack Tests =====

void test_pub_ack_success() {
    TEST("parse_pub_ack: valid ACK");
    const char* json = R"({"stream":"test-stream","seq":42,"duplicate":false})";
    const char* stream = NULL;
    uint64_t seq = 0;
    bool dup = true;
    const char* error = NULL;

    bool ok = NATSParsers::parse_pub_ack(json, strlen(json), &stream, &seq, &dup, &error);
    ASSERT(ok, "should succeed");
    ASSERT(stream != NULL, "stream not null");
    ASSERT(strcmp(stream, "test-stream") == 0, "stream name matches");
    ASSERT(seq == 42, "sequence number");
    ASSERT(dup == false, "not duplicate");

    free((void*)stream);
    PASS();
}

void test_pub_ack_duplicate() {
    TEST("parse_pub_ack: duplicate message");
    const char* json = R"({"stream":"mystream","seq":100,"duplicate":true})";
    const char* stream = NULL;
    uint64_t seq = 0;
    bool dup = false;
    const char* error = NULL;

    bool ok = NATSParsers::parse_pub_ack(json, strlen(json), &stream, &seq, &dup, &error);
    ASSERT(ok, "should succeed");
    ASSERT(seq == 100, "seq");
    ASSERT(dup == true, "should be duplicate");

    free((void*)stream);
    PASS();
}

void test_pub_ack_large_seq() {
    TEST("parse_pub_ack: large sequence number");
    const char* json = R"({"stream":"s","seq":9007199254740992})";
    const char* stream = NULL;
    uint64_t seq = 0;
    bool dup = false;
    const char* error = NULL;

    bool ok = NATSParsers::parse_pub_ack(json, strlen(json), &stream, &seq, &dup, &error);
    ASSERT(ok, "should succeed");
    // 2^53 = 9007199254740992 is at the edge of double precision
    ASSERT(seq > 0, "seq should be non-zero");

    free((void*)stream);
    PASS();
}

void test_pub_ack_error_response() {
    TEST("parse_pub_ack: error response");
    const char* json = R"({"error":{"code":400,"description":"bad request"}})";
    const char* stream = NULL;
    uint64_t seq = 0;
    bool dup = false;
    const char* error = NULL;

    bool ok = NATSParsers::parse_pub_ack(json, strlen(json), &stream, &seq, &dup, &error);
    ASSERT(!ok, "should fail for error response");
    ASSERT(error != NULL, "error desc not null");
    ASSERT(strcmp(error, "bad request") == 0, "error message matches");

    free((void*)error);
    PASS();
}

void test_pub_ack_error_no_description() {
    TEST("parse_pub_ack: error without description");
    const char* json = R"({"error":{"code":503}})";
    const char* error = NULL;

    bool ok = NATSParsers::parse_pub_ack(json, strlen(json), NULL, NULL, NULL, &error);
    ASSERT(!ok, "should fail");
    ASSERT(error != NULL, "should have fallback error");
    ASSERT(strcmp(error, "Unknown error") == 0, "fallback message");

    free((void*)error);
    PASS();
}

void test_pub_ack_invalid_json() {
    TEST("parse_pub_ack: invalid JSON");
    const char* json = "not json at all {{{";
    bool ok = NATSParsers::parse_pub_ack(json, strlen(json), NULL, NULL, NULL, NULL);
    ASSERT(!ok, "should fail on invalid JSON");
    PASS();
}

void test_pub_ack_null_input() {
    TEST("parse_pub_ack: NULL and empty input");
    ASSERT(!NATSParsers::parse_pub_ack(NULL, 0, NULL, NULL, NULL, NULL), "NULL data");
    ASSERT(!NATSParsers::parse_pub_ack("", 0, NULL, NULL, NULL, NULL), "zero size");
    ASSERT(!NATSParsers::parse_pub_ack("x", -1, NULL, NULL, NULL, NULL), "negative size");
    PASS();
}

void test_pub_ack_null_outputs() {
    TEST("parse_pub_ack: NULL output params (no crash)");
    const char* json = R"({"stream":"s","seq":1})";
    bool ok = NATSParsers::parse_pub_ack(json, strlen(json), NULL, NULL, NULL, NULL);
    ASSERT(ok, "should succeed even with all NULL outputs");
    PASS();
}

void test_pub_ack_minimal_json() {
    TEST("parse_pub_ack: minimal JSON (empty object)");
    const char* json = "{}";
    const char* stream = NULL;
    uint64_t seq = 0;
    bool ok = NATSParsers::parse_pub_ack(json, strlen(json), &stream, &seq, NULL, NULL);
    // No error field, but also no stream/seq — implementation may return true or false
    // The key thing is it doesn't crash
    (void)ok;
    if (stream) free((void*)stream);
    PASS();
}

// ===== parse_account_info Tests =====

void test_account_info_full() {
    TEST("parse_account_info: full response");
    const char* json = R"({
        "memory": 1024,
        "storage": 2048,
        "streams": 5,
        "consumers": 10,
        "limits": {
            "max_memory": 1048576,
            "max_storage": 10485760,
            "max_streams": 100,
            "max_consumers": 1000
        }
    })";

    jetstream_account_info_t info;
    bool ok = NATSParsers::parse_account_info(json, strlen(json), &info);
    ASSERT(ok, "should succeed");
    ASSERT(info.memory == 1024, "memory");
    ASSERT(info.storage == 2048, "storage");
    ASSERT(info.streams == 5, "streams");
    ASSERT(info.consumers == 10, "consumers");
    ASSERT(info.memory_max == 1048576, "memory_max");
    ASSERT(info.storage_max == 10485760, "storage_max");
    ASSERT(info.streams_max == 100, "streams_max");
    ASSERT(info.consumers_max == 1000, "consumers_max");
    PASS();
}

void test_account_info_no_limits() {
    TEST("parse_account_info: no limits section");
    const char* json = R"({"memory":512,"storage":1024,"streams":1,"consumers":2})";
    jetstream_account_info_t info;
    bool ok = NATSParsers::parse_account_info(json, strlen(json), &info);
    ASSERT(ok, "should succeed without limits");
    ASSERT(info.memory == 512, "memory");
    ASSERT(info.memory_max == 0, "no limits = 0");
    PASS();
}

void test_account_info_error() {
    TEST("parse_account_info: error response");
    const char* json = R"({"error":{"code":503,"description":"unavailable"}})";
    jetstream_account_info_t info;
    bool ok = NATSParsers::parse_account_info(json, strlen(json), &info);
    ASSERT(!ok, "should fail on error response");
    PASS();
}

void test_account_info_null() {
    TEST("parse_account_info: NULL inputs");
    jetstream_account_info_t info;
    ASSERT(!NATSParsers::parse_account_info(NULL, 0, &info), "NULL data");
    ASSERT(!NATSParsers::parse_account_info("{}", 2, NULL), "NULL info");
    PASS();
}

// ===== parse_object_meta Tests =====

void test_object_meta_full() {
    TEST("parse_object_meta: full metadata");
    const char* json = R"({
        "name": "firmware.bin",
        "description": "OTA update v2.1",
        "size": 131072,
        "chunks": 2,
        "digest": "sha256=abc123def456",
        "mtime": 1700000000000
    })";

    object_meta_t meta = {};
    bool ok = NATSParsers::parse_object_meta(json, strlen(json), &meta);
    ASSERT(ok, "should succeed");
    ASSERT(meta.name != NULL && strcmp(meta.name, "firmware.bin") == 0, "name");
    ASSERT(meta.description != NULL && strcmp(meta.description, "OTA update v2.1") == 0, "description");
    ASSERT(meta.size == 131072, "size");
    ASSERT(meta.chunks == 2, "chunks");
    ASSERT(meta.digest != NULL && strcmp(meta.digest, "sha256=abc123def456") == 0, "digest");
    ASSERT(meta.mtime > 0, "mtime non-zero");

    free((void*)meta.name);
    free((void*)meta.description);
    free((void*)meta.digest);
    PASS();
}

void test_object_meta_minimal() {
    TEST("parse_object_meta: only required fields");
    const char* json = R"({"name":"test.txt","size":100,"chunks":1})";
    object_meta_t meta = {};
    bool ok = NATSParsers::parse_object_meta(json, strlen(json), &meta);
    ASSERT(ok, "should succeed");
    ASSERT(meta.name != NULL && strcmp(meta.name, "test.txt") == 0, "name");
    ASSERT(meta.size == 100, "size");
    ASSERT(meta.description == NULL, "no description");
    ASSERT(meta.digest == NULL, "no digest");

    free((void*)meta.name);
    PASS();
}

void test_object_meta_null() {
    TEST("parse_object_meta: NULL inputs");
    object_meta_t meta;
    ASSERT(!NATSParsers::parse_object_meta(NULL, 0, &meta), "NULL data");
    ASSERT(!NATSParsers::parse_object_meta("{}", 2, NULL), "NULL meta");
    PASS();
}

// ===== is_error_response Tests =====

void test_is_error_yes() {
    TEST("is_error_response: error present");
    const char* json = R"({"error":{"code":404,"description":"stream not found"}})";
    int code = 0;
    const char* desc = NULL;

    bool is_err = NATSParsers::is_error_response(json, strlen(json), &code, &desc);
    ASSERT(is_err, "should be error");
    ASSERT(code == 404, "error code");
    ASSERT(desc != NULL && strcmp(desc, "stream not found") == 0, "error desc");

    free((void*)desc);
    PASS();
}

void test_is_error_no() {
    TEST("is_error_response: success response");
    const char* json = R"({"stream":"test","seq":1})";
    int code = 999;
    const char* desc = (const char*)0xDEAD;

    bool is_err = NATSParsers::is_error_response(json, strlen(json), &code, &desc);
    ASSERT(!is_err, "should not be error");
    ASSERT(code == 0, "code zeroed on non-error");
    ASSERT(desc == NULL, "desc NULL on non-error");
    PASS();
}

void test_is_error_null_outputs() {
    TEST("is_error_response: NULL output params");
    const char* json = R"({"error":{"code":500}})";
    bool is_err = NATSParsers::is_error_response(json, strlen(json), NULL, NULL);
    ASSERT(is_err, "should detect error even with NULL outputs");
    PASS();
}

void test_is_error_null_input() {
    TEST("is_error_response: NULL input");
    ASSERT(!NATSParsers::is_error_response(NULL, 0, NULL, NULL), "NULL");
    ASSERT(!NATSParsers::is_error_response("", 0, NULL, NULL), "empty");
    PASS();
}

void test_is_error_invalid_json() {
    TEST("is_error_response: invalid JSON");
    ASSERT(!NATSParsers::is_error_response("{bad", 4, NULL, NULL), "invalid JSON");
    PASS();
}

void test_is_error_nested_error_key() {
    TEST("is_error_response: error key in nested object");
    // Only top-level "error" should count
    const char* json = R"({"data":{"error":"not a real error"},"stream":"s"})";
    bool is_err = NATSParsers::is_error_response(json, strlen(json), NULL, NULL);
    ASSERT(!is_err, "nested error key should not trigger");
    PASS();
}

// ===== Main =====

int main() {
    printf("=== NATSParsers Host Tests ===\n\n");

    printf("--- parse_pub_ack ---\n");
    test_pub_ack_success();
    test_pub_ack_duplicate();
    test_pub_ack_large_seq();
    test_pub_ack_error_response();
    test_pub_ack_error_no_description();
    test_pub_ack_invalid_json();
    test_pub_ack_null_input();
    test_pub_ack_null_outputs();
    test_pub_ack_minimal_json();

    printf("\n--- parse_account_info ---\n");
    test_account_info_full();
    test_account_info_no_limits();
    test_account_info_error();
    test_account_info_null();

    printf("\n--- parse_object_meta ---\n");
    test_object_meta_full();
    test_object_meta_minimal();
    test_object_meta_null();

    printf("\n--- is_error_response ---\n");
    test_is_error_yes();
    test_is_error_no();
    test_is_error_null_outputs();
    test_is_error_null_input();
    test_is_error_invalid_json();
    test_is_error_nested_error_key();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");
    return tests_failed > 0 ? 1 : 0;
}
