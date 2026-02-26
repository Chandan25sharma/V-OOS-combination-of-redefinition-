/*
 * VOS Unit Test — Privacy Engine
 */
#include <cassert>
#include <cstdio>
#include <thread>
#include <chrono>
#include "core/privacy.h"

using namespace vos;

void test_init_and_identity() {
    PrivacyEngine pe;
    auto r = pe.init(10);
    assert(r.ok());
    assert(pe.is_running());

    auto id = pe.get_current_identity();
    assert(!id.virtual_ip.empty());
    assert(!id.virtual_mac.empty());
    assert(id.rotation_count >= 1);

    pe.shutdown();
    assert(!pe.is_running());
    printf("[PASS] test_init_and_identity\n");
}

void test_force_rotate() {
    PrivacyEngine pe;
    pe.init(60); // Long interval so auto-rotate doesn't interfere

    auto id1 = pe.get_current_identity();
    auto count_before = id1.rotation_count;

    pe.force_rotate();
    auto id2 = pe.get_current_identity();
    assert(id2.rotation_count == count_before + 1);
    assert(id2.virtual_ip != id1.virtual_ip || id2.virtual_mac != id1.virtual_mac);

    pe.shutdown();
    printf("[PASS] test_force_rotate\n");
}

void test_callback() {
    PrivacyEngine pe;
    int callback_count = 0;

    pe.on_identity_changed([&](const IdentityState& state) {
        callback_count++;
    });

    pe.init(1); // Rotate every 1 second for test speed
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    pe.shutdown();

    assert(callback_count >= 1);
    printf("[PASS] test_callback (received %d rotations)\n", callback_count);
}

void test_double_init() {
    PrivacyEngine pe;
    auto r1 = pe.init(10);
    assert(r1.ok());
    auto r2 = pe.init(10);
    assert(!r2.ok());
    assert(r2.status == StatusCode::ERR_ALREADY_EXISTS);
    pe.shutdown();
    printf("[PASS] test_double_init\n");
}

int main() {
    printf("=== Privacy Engine Tests ===\n");
    test_init_and_identity();
    test_force_rotate();
    test_callback();
    test_double_init();
    printf("All Privacy Engine tests passed!\n\n");
    return 0;
}
