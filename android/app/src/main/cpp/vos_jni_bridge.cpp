/*
 * VOS — JNI Bridge
 * Bridges the C++ core engine to the Android Java/Kotlin layer.
 */

#include <jni.h>
#include <string>
#include <android/log.h>

#include "core/kernel.h"
#include "core/vfs.h"
#include "core/crypto.h"
#include "core/privacy.h"
#include "core/mesh_net.h"
#include "core/lockdown.h"
#include "apps/dialer.h"
#include "apps/sms.h"
#include "apps/camera.h"

#define LOG_TAG "VOS_JNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ─── Global instances ────────────────────────────────────────
static vos::Kernel          g_kernel;
static vos::VirtualFS       g_vfs;
static vos::Crypto          g_crypto;
static vos::PrivacyEngine   g_privacy;
static vos::MeshNet         g_mesh;
static vos::LockdownManager g_lockdown;
static vos::Dialer          g_dialer;
static vos::SmsApp          g_sms;
static vos::CameraApp       g_camera;

static JavaVM* g_jvm = nullptr;

// Helper to get JNIEnv from any thread
static JNIEnv* get_env() {
    JNIEnv* env = nullptr;
    if (g_jvm) g_jvm->AttachCurrentThread(&env, nullptr);
    return env;
}

extern "C" {

// ═══════════════════════════════════════════════════════════════
// LIFECYCLE
// ═══════════════════════════════════════════════════════════════

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    LOGI("JNI_OnLoad: VOS native library loaded");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_init(JNIEnv* env, jobject thiz) {
    LOGI("Initializing VOS core...");
    g_crypto.init();
    g_kernel.init();
    g_vfs.init();
    g_privacy.init(10); // 10-second IP rotation
    g_mesh.init(&g_crypto);
    g_mesh.start_discovery();
    g_lockdown.init();
    g_dialer.init();
    g_sms.init();
    g_camera.init();

    // Wire mesh → SMS
    g_mesh.on_message([](const std::string& peer_id, const vos::ByteBuffer& payload) {
        std::string text(payload.begin(), payload.end());
        g_sms.receive(peer_id, text);
    });

    LOGI("VOS core initialized successfully");
}

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_shutdown(JNIEnv* env, jobject thiz) {
    g_camera.close();
    g_mesh.shutdown();
    g_privacy.shutdown();
    g_kernel.shutdown();
    LOGI("VOS core shutdown");
}

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_tick(JNIEnv* env, jobject thiz) {
    g_kernel.tick();
    g_dialer.tick();
}

// ═══════════════════════════════════════════════════════════════
// PRIVACY ENGINE
// ═══════════════════════════════════════════════════════════════

JNIEXPORT jstring JNICALL
Java_com_vos_app_NativeEngine_getVirtualIP(JNIEnv* env, jobject thiz) {
    auto id = g_privacy.get_current_identity();
    return env->NewStringUTF(id.virtual_ip.c_str());
}

JNIEXPORT jstring JNICALL
Java_com_vos_app_NativeEngine_getVirtualMAC(JNIEnv* env, jobject thiz) {
    auto id = g_privacy.get_current_identity();
    return env->NewStringUTF(id.virtual_mac.c_str());
}

JNIEXPORT jlong JNICALL
Java_com_vos_app_NativeEngine_getRotationCount(JNIEnv* env, jobject thiz) {
    auto id = g_privacy.get_current_identity();
    return (jlong)id.rotation_count;
}

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_forceRotate(JNIEnv* env, jobject thiz) {
    g_privacy.force_rotate();
}

// ═══════════════════════════════════════════════════════════════
// LOCKDOWN
// ═══════════════════════════════════════════════════════════════

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_startLockdown(JNIEnv* env, jobject thiz, jint seconds) {
    g_lockdown.start(vos::Seconds(seconds));
}

JNIEXPORT jboolean JNICALL
Java_com_vos_app_NativeEngine_isLockdownActive(JNIEnv* env, jobject thiz) {
    return g_lockdown.is_active() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_com_vos_app_NativeEngine_getLockdownRemaining(JNIEnv* env, jobject thiz) {
    return (jlong)g_lockdown.get_remaining_time().count();
}

JNIEXPORT jboolean JNICALL
Java_com_vos_app_NativeEngine_isAppAllowed(JNIEnv* env, jobject thiz, jint appId) {
    return g_lockdown.is_app_allowed((vos::AppId)appId) ? JNI_TRUE : JNI_FALSE;
}

// ═══════════════════════════════════════════════════════════════
// MESH NETWORK
// ═══════════════════════════════════════════════════════════════

JNIEXPORT jstring JNICALL
Java_com_vos_app_NativeEngine_getOwnPeerId(JNIEnv* env, jobject thiz) {
    return env->NewStringUTF(g_mesh.get_own_id().c_str());
}

JNIEXPORT jint JNICALL
Java_com_vos_app_NativeEngine_getPeerCount(JNIEnv* env, jobject thiz) {
    return (jint)g_mesh.get_peers().size();
}

JNIEXPORT jobjectArray JNICALL
Java_com_vos_app_NativeEngine_getPeerIds(JNIEnv* env, jobject thiz) {
    auto peers = g_mesh.get_peers();
    jobjectArray arr = env->NewObjectArray((jsize)peers.size(),
        env->FindClass("java/lang/String"), nullptr);
    for (int i = 0; i < (int)peers.size(); i++) {
        env->SetObjectArrayElement(arr, i, env->NewStringUTF(peers[i].peer_id.c_str()));
    }
    return arr;
}

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_sendMeshText(JNIEnv* env, jobject thiz,
                                            jstring peerId, jstring message) {
    const char* pid = env->GetStringUTFChars(peerId, nullptr);
    const char* msg = env->GetStringUTFChars(message, nullptr);
    g_sms.send(pid, msg);
    g_mesh.send_text(pid, msg);
    env->ReleaseStringUTFChars(peerId, pid);
    env->ReleaseStringUTFChars(message, msg);
}

// ═══════════════════════════════════════════════════════════════
// DIALER
// ═══════════════════════════════════════════════════════════════

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_dial(JNIEnv* env, jobject thiz, jstring number) {
    const char* num = env->GetStringUTFChars(number, nullptr);
    g_dialer.dial(num);
    env->ReleaseStringUTFChars(number, num);
}

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_hangUp(JNIEnv* env, jobject thiz) {
    g_dialer.hang_up();
}

JNIEXPORT jint JNICALL
Java_com_vos_app_NativeEngine_getCallState(JNIEnv* env, jobject thiz) {
    return (jint)g_dialer.get_state();
}

JNIEXPORT jint JNICALL
Java_com_vos_app_NativeEngine_getCallDuration(JNIEnv* env, jobject thiz) {
    return g_dialer.get_call_duration();
}

// ═══════════════════════════════════════════════════════════════
// SMS
// ═══════════════════════════════════════════════════════════════

JNIEXPORT jint JNICALL
Java_com_vos_app_NativeEngine_getTotalUnread(JNIEnv* env, jobject thiz) {
    return g_sms.total_unread();
}

// ═══════════════════════════════════════════════════════════════
// CAMERA
// ═══════════════════════════════════════════════════════════════

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_openCamera(JNIEnv* env, jobject thiz) {
    g_camera.open();
}

JNIEXPORT void JNICALL
Java_com_vos_app_NativeEngine_closeCamera(JNIEnv* env, jobject thiz) {
    g_camera.close();
}

JNIEXPORT jint JNICALL
Java_com_vos_app_NativeEngine_getCaptureCount(JNIEnv* env, jobject thiz) {
    return (jint)g_camera.capture_count();
}

} // extern "C"
