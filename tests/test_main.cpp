#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

// Forward declarations of test functions
extern void test_buffer();
extern void test_w3i();
extern void test_w3u();
extern void test_w3e();
extern void test_doo();
extern void test_units_doo();
extern void test_wts();
extern void test_slk();
extern void test_archive();
extern void test_object_id_gen();

static int tests_run = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    try { \
        name(); \
        printf("  PASS: %s\n", #name); \
    } catch (const std::exception& e) { \
        tests_failed++; \
        printf("  FAIL: %s — %s\n", #name, e.what()); \
    } catch (...) { \
        tests_failed++; \
        printf("  FAIL: %s — unknown exception\n", #name); \
    } \
} while(0)

#define ASSERT(cond, msg) do { \
    if (!(cond)) throw std::runtime_error(msg); \
} while(0)

// ============================================================
// Main
// ============================================================
int main() {
    printf("w3x-packer tests\n");
    printf("================\n\n");

    TEST(test_buffer);
    TEST(test_w3i);
    TEST(test_w3u);
    TEST(test_wts);
    TEST(test_slk);
    TEST(test_w3e);
    TEST(test_doo);
    TEST(test_units_doo);
    TEST(test_archive);
    TEST(test_object_id_gen);

    printf("\n%d tests, %d passed, %d failed\n",
           tests_run, tests_run - tests_failed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
