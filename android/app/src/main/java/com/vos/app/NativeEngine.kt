package com.vos.app

/**
 * VOS — Native Engine Bridge
 * Kotlin interface to the C++ VOS core via JNI.
 */
class NativeEngine {

    companion object {
        init {
            System.loadLibrary("vos_native")
        }
    }

    // ─── Lifecycle ───────────────────────────────────────────
    external fun init()
    external fun shutdown()
    external fun tick()

    // ─── Privacy ─────────────────────────────────────────────
    external fun getVirtualIP(): String
    external fun getVirtualMAC(): String
    external fun getRotationCount(): Long
    external fun forceRotate()

    // ─── Lockdown ────────────────────────────────────────────
    external fun startLockdown(seconds: Int)
    external fun isLockdownActive(): Boolean
    external fun getLockdownRemaining(): Long
    external fun isAppAllowed(appId: Int): Boolean

    // ─── Mesh Network ────────────────────────────────────────
    external fun getOwnPeerId(): String
    external fun getPeerCount(): Int
    external fun getPeerIds(): Array<String>
    external fun sendMeshText(peerId: String, message: String)

    // ─── Dialer ──────────────────────────────────────────────
    external fun dial(number: String)
    external fun hangUp()
    external fun getCallState(): Int
    external fun getCallDuration(): Int

    // ─── SMS ─────────────────────────────────────────────────
    external fun getTotalUnread(): Int

    // ─── Camera ──────────────────────────────────────────────
    external fun openCamera()
    external fun closeCamera()
    external fun getCaptureCount(): Int

    // ─── App ID Constants (mirror types.h) ───────────────────
    object AppIds {
        const val SYSTEM = 0
        const val DIALER = 1
        const val SMS    = 2
        const val CAMERA = 3
    }

    // ─── Call State Constants (mirror dialer.h) ──────────────
    object CallStates {
        const val IDLE    = 0
        const val DIALING = 1
        const val RINGING = 2
        const val IN_CALL = 3
        const val ENDED   = 4
        const val MISSED  = 5
    }
}
