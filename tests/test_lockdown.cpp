/*
 * VOS Unit Test — Lockdown Manager
 */
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include "core/lockdown.h"

using namespace vos;

void test_init() {
    LockdownManager lm;
    auto r = lm.init();
    assert(r.ok());
    assert(!lm.is_active());
    printf("[PASS] test_init\n");
}

void test_whitelist() {
    LockdownManager lm;
    lm.init();
    lm.start(Seconds(60));
    assert(lm.is_active());

    // Whitelisted apps
    assert(lm.is_app_allowed(APP_DIALER));
    assert(lm.is_app_allowed(APP_SMS));
    assert(lm.is_app_allowed(APP_CAMERA));
    assert(lm.is_app_allowed(APP_SYSTEM));

    // Non-whitelisted app (hypothetical ID 99)
    assert(!lm.is_app_allowed(99));

    lm.force_unlock();
    printf("[PASS] test_whitelist\n");
}

void test_timer_expiry() {
    LockdownManager lm;
    lm.init();

    // Start a 1-second lockdown
    lm.start(Seconds(1));
    assert(lm.is_active());

    // Wait for it to expire
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    assert(!lm.is_active());
    printf("[PASS] test_timer_expiry\n");
}

void test_remaining_time() {
    LockdownManager lm;
    lm.init();
    lm.start(Seconds(10));

    auto rem = lm.get_remaining_time();
    assert(rem.count() > 0);
    assert(rem.count() <= 10);

    lm.force_unlock();
    rem = lm.get_remaining_time();
    assert(rem.count() == 0);
    printf("[PASS] test_remaining_time\n");
}

void test_force_unlock() {
    LockdownManager lm;
    lm.init();
    lm.start(Seconds(3600)); // 1 hour
    assert(lm.is_active());

    lm.force_unlock();
    assert(!lm.is_active());
    printf("[PASS] test_force_unlock\n");
}

void test_double_start() {
    LockdownManager lm;
    lm.init();
    auto r1 = lm.start(Seconds(60));
    assert(r1.ok());
    auto r2 = lm.start(Seconds(60));
    assert(!r2.ok());
    assert(r2.status == StatusCode::ERR_ALREADY_EXISTS);
    lm.force_unlock();
    printf("[PASS] test_double_start\n");
}

void test_unlocked_allows_all() {
    LockdownManager lm;
    lm.init();
    // When not locked, all apps should be allowed
    assert(lm.is_app_allowed(APP_DIALER));
    assert(lm.is_app_allowed(99)); // Even arbitrary IDs
    printf("[PASS] test_unlocked_allows_all\n");
}

int main() {
    printf("=== Lockdown Manager Tests ===\n");
    test_init();
    test_whitelist();
    test_timer_expiry();
    test_remaining_time();
    test_force_unlock();
    test_double_start();
    test_unlocked_allows_all();
    printf("All Lockdown Manager tests passed!\n\n");
    return 0;
}
