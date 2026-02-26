package com.vos.app

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.View
import android.view.WindowManager
import android.widget.*

/**
 * VOS — Main Launcher Activity
 * Acts as the home screen / launcher for the VOS virtual OS.
 * Only shows Phone, SMS, Camera, and System Info.
 */
class MainActivity : Activity() {

    private val engine = NativeEngine()
    private val handler = Handler(Looper.getMainLooper())
    private val tickInterval = 500L // ms

    // UI Elements
    private lateinit var statusBar: TextView
    private lateinit var lockdownStatus: TextView
    private lateinit var btnPhone: Button
    private lateinit var btnSms: Button
    private lateinit var btnCamera: Button
    private lateinit var btnLockdown: Button
    private lateinit var lockdownSlider: SeekBar
    private lateinit var lockdownLabel: TextView
    private lateinit var peerCount: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Keep screen on during VOS
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Initialize native engine
        engine.init()

        // Build UI programmatically (no XML layout dependency)
        buildUI()

        // Start tick loop
        handler.post(tickRunnable)
    }

    override fun onDestroy() {
        handler.removeCallbacks(tickRunnable)
        engine.shutdown()
        super.onDestroy()
    }

    // ─── Tick Loop ───────────────────────────────────────────
    private val tickRunnable = object : Runnable {
        override fun run() {
            engine.tick()
            updateUI()
            handler.postDelayed(this, tickInterval)
        }
    }

    private fun updateUI() {
        val ip = engine.getVirtualIP()
        val mac = engine.getVirtualMAC()
        val rotations = engine.getRotationCount()
        val peers = engine.getPeerCount()

        statusBar.text = "VOS | IP: $ip | MAC: $mac | #$rotations"
        peerCount.text = "Peers: $peers"

        val locked = engine.isLockdownActive()
        if (locked) {
            val remaining = engine.getLockdownRemaining()
            val mins = remaining / 60
            val secs = remaining % 60
            lockdownStatus.text = "LOCKED — ${String.format("%02d:%02d", mins, secs)}"
            lockdownStatus.setTextColor(0xFFFF4444.toInt())
            btnLockdown.isEnabled = false
            lockdownSlider.isEnabled = false

            // Disable blocked apps visually
            btnPhone.alpha = if (engine.isAppAllowed(NativeEngine.AppIds.DIALER)) 1.0f else 0.3f
            btnSms.alpha = if (engine.isAppAllowed(NativeEngine.AppIds.SMS)) 1.0f else 0.3f
            btnCamera.alpha = if (engine.isAppAllowed(NativeEngine.AppIds.CAMERA)) 1.0f else 0.3f
        } else {
            lockdownStatus.text = "UNLOCKED"
            lockdownStatus.setTextColor(0xFF44FF88.toInt())
            btnLockdown.isEnabled = true
            lockdownSlider.isEnabled = true
            btnPhone.alpha = 1.0f
            btnSms.alpha = 1.0f
            btnCamera.alpha = 1.0f
        }

        // Unread badge
        val unread = engine.getTotalUnread()
        btnSms.text = if (unread > 0) "SMS ($unread)" else "SMS"
    }

    // ─── Build UI ────────────────────────────────────────────
    private fun buildUI() {
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF0E0E12.toInt())
            setPadding(24, 16, 24, 16)
        }

        // Status bar
        statusBar = TextView(this).apply {
            text = "VOS | Initializing..."
            setTextColor(0xFF4DA6FF.toInt())
            textSize = 12f
            setPadding(0, 0, 0, 8)
        }
        root.addView(statusBar)

        // Peer count
        peerCount = TextView(this).apply {
            text = "Peers: 0"
            setTextColor(0xFFAAAAAA.toInt())
            textSize = 11f
        }
        root.addView(peerCount)

        // Divider
        root.addView(View(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 2
            ).also { it.setMargins(0, 12, 0, 20) }
            setBackgroundColor(0xFF333344.toInt())
        })

        // App title
        root.addView(TextView(this).apply {
            text = "VOS Applications"
            setTextColor(0xFFDDDDEE.toInt())
            textSize = 18f
            setPadding(0, 0, 0, 16)
        })

        // App buttons row
        val appRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, 0, 0, 24)
        }

        val btnParams = LinearLayout.LayoutParams(0, 160, 1f).also {
            it.setMargins(8, 0, 8, 0)
        }

        btnPhone = Button(this).apply {
            text = "PHONE"
            textSize = 16f
            setTextColor(0xFFFFFFFF.toInt())
            setBackgroundColor(0xFF1A5C2E.toInt())
            layoutParams = btnParams
            setOnClickListener { onPhoneClick() }
        }
        appRow.addView(btnPhone)

        btnSms = Button(this).apply {
            text = "SMS"
            textSize = 16f
            setTextColor(0xFFFFFFFF.toInt())
            setBackgroundColor(0xFF1A3B6D.toInt())
            layoutParams = LinearLayout.LayoutParams(0, 160, 1f).also {
                it.setMargins(8, 0, 8, 0)
            }
            setOnClickListener { onSmsClick() }
        }
        appRow.addView(btnSms)

        btnCamera = Button(this).apply {
            text = "CAMERA"
            textSize = 16f
            setTextColor(0xFFFFFFFF.toInt())
            setBackgroundColor(0xFF5C3D1A.toInt())
            layoutParams = LinearLayout.LayoutParams(0, 160, 1f).also {
                it.setMargins(8, 0, 8, 0)
            }
            setOnClickListener { onCameraClick() }
        }
        appRow.addView(btnCamera)

        root.addView(appRow)

        // Lockdown section divider
        root.addView(View(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 1
            ).also { it.setMargins(0, 8, 0, 16) }
            setBackgroundColor(0xFF333344.toInt())
        })

        root.addView(TextView(this).apply {
            text = "Lockdown Mode"
            setTextColor(0xFFFF6666.toInt())
            textSize = 16f
            setPadding(0, 0, 0, 8)
        })

        lockdownStatus = TextView(this).apply {
            text = "UNLOCKED"
            setTextColor(0xFF44FF88.toInt())
            textSize = 14f
            setPadding(0, 0, 0, 12)
        }
        root.addView(lockdownStatus)

        // Duration slider
        lockdownLabel = TextView(this).apply {
            text = "Duration: 5 min"
            setTextColor(0xFFBBBBBB.toInt())
            textSize = 13f
        }
        root.addView(lockdownLabel)

        lockdownSlider = SeekBar(this).apply {
            max = 119  // 1-120 min
            progress = 4 // default 5 min
            setPadding(0, 8, 0, 12)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, progress: Int, user: Boolean) {
                    lockdownLabel.text = "Duration: ${progress + 1} min"
                }
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }
        root.addView(lockdownSlider)

        btnLockdown = Button(this).apply {
            text = "ACTIVATE LOCKDOWN"
            textSize = 14f
            setTextColor(0xFFFFFFFF.toInt())
            setBackgroundColor(0xFFCC2222.toInt())
            setPadding(16, 12, 16, 12)
            setOnClickListener {
                val minutes = lockdownSlider.progress + 1
                engine.startLockdown(minutes * 60)
                // Start lockdown service
                val intent = Intent(this@MainActivity, LockdownService::class.java)
                intent.putExtra("duration_seconds", minutes * 60)
                startService(intent)
            }
        }
        root.addView(btnLockdown)

        // System info at bottom
        root.addView(View(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 1
            ).also { it.setMargins(0, 24, 0, 8) }
            setBackgroundColor(0xFF333344.toInt())
        })

        root.addView(TextView(this).apply {
            text = "VOS — Virtual OS v0.1.0\nPrivacy-first portable OS environment"
            setTextColor(0xFF666677.toInt())
            textSize = 11f
        })

        setContentView(root)
    }

    // ─── App Actions ─────────────────────────────────────────
    private fun onPhoneClick() {
        if (!engine.isAppAllowed(NativeEngine.AppIds.DIALER)) {
            Toast.makeText(this, "Phone is locked during lockdown", Toast.LENGTH_SHORT).show()
            return
        }
        // Launch system dialer
        val intent = Intent(Intent.ACTION_DIAL)
        startActivity(intent)
    }

    private fun onSmsClick() {
        if (!engine.isAppAllowed(NativeEngine.AppIds.SMS)) {
            Toast.makeText(this, "Messages locked during lockdown", Toast.LENGTH_SHORT).show()
            return
        }
        // Launch system SMS
        val intent = Intent(Intent.ACTION_VIEW, Uri.parse("sms:"))
        startActivity(intent)
    }

    private fun onCameraClick() {
        if (!engine.isAppAllowed(NativeEngine.AppIds.CAMERA)) {
            Toast.makeText(this, "Camera locked during lockdown", Toast.LENGTH_SHORT).show()
            return
        }
        // Launch system camera
        val intent = Intent("android.media.action.IMAGE_CAPTURE")
        startActivity(intent)
    }

    // ─── Prevent leaving VOS during lockdown ─────────────────
    override fun onBackPressed() {
        if (engine.isLockdownActive()) {
            Toast.makeText(this, "Cannot exit — Lockdown active", Toast.LENGTH_SHORT).show()
        } else {
            super.onBackPressed()
        }
    }
}
