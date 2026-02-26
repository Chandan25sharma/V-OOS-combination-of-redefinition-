package com.vos.app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.app.usage.UsageStatsManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper

/**
 * VOS — Lockdown Service
 * Runs as a foreground service during lockdown period.
 * Monitors foreground app and redirects user back to VOS
 * if they try to open anything outside the whitelist.
 */
class LockdownService : Service() {

    private val handler = Handler(Looper.getMainLooper())
    private var durationSeconds = 0
    private var elapsedSeconds = 0
    private val engine = NativeEngine()

    private val CHANNEL_ID = "vos_lockdown"
    private val NOTIFICATION_ID = 1001

    // Whitelisted packages (system Phone, SMS, Camera + VOS itself)
    private val whitelistedPackages = setOf(
        "com.vos.app",
        "com.android.dialer",
        "com.google.android.dialer",
        "com.android.phone",
        "com.samsung.android.dialer",
        "com.android.mms",
        "com.google.android.apps.messaging",
        "com.samsung.android.messaging",
        "com.android.camera",
        "com.android.camera2",
        "com.google.android.GoogleCamera",
        "com.samsung.android.camera",
        "com.sec.android.app.camera",
        // System UI (necessary for notifications, quick settings)
        "com.android.systemui",
        "android"
    )

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        durationSeconds = intent?.getIntExtra("duration_seconds", 300) ?: 300
        elapsedSeconds = 0

        // Start as foreground service
        val notification = buildNotification("VOS Lockdown Active — ${durationSeconds / 60} min remaining")
        startForeground(NOTIFICATION_ID, notification)

        // Start monitoring loop
        handler.post(monitorRunnable)

        return START_STICKY
    }

    private val monitorRunnable = object : Runnable {
        override fun run() {
            elapsedSeconds++

            if (elapsedSeconds >= durationSeconds) {
                // Lockdown expired
                stopSelf()
                return
            }

            // Check current foreground app
            checkForegroundApp()

            // Update notification
            val remaining = durationSeconds - elapsedSeconds
            val mins = remaining / 60
            val secs = remaining % 60
            val notification = buildNotification(
                "VOS Lockdown — ${String.format("%02d:%02d", mins, secs)} remaining"
            )
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.notify(NOTIFICATION_ID, notification)

            handler.postDelayed(this, 1000)
        }
    }

    private fun checkForegroundApp() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP_MR1) {
            val usm = getSystemService(Context.USAGE_STATS_SERVICE) as? UsageStatsManager
            val endTime = System.currentTimeMillis()
            val startTime = endTime - 5000

            val stats = usm?.queryUsageStats(
                UsageStatsManager.INTERVAL_BEST, startTime, endTime
            )

            if (stats != null && stats.isNotEmpty()) {
                val sorted = stats.sortedByDescending { it.lastTimeUsed }
                val topPackage = sorted.firstOrNull()?.packageName

                if (topPackage != null && topPackage !in whitelistedPackages) {
                    // Non-whitelisted app is in foreground — redirect to VOS
                    val intent = Intent(this, MainActivity::class.java).apply {
                        addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP)
                    }
                    startActivity(intent)
                }
            }
        }
    }

    override fun onDestroy() {
        handler.removeCallbacks(monitorRunnable)
        super.onDestroy()
    }

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "VOS Lockdown",
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Active during VOS lockdown period"
                setShowBadge(false)
            }
            val nm = getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            nm.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(text: String): Notification {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, CHANNEL_ID)
                .setContentTitle("VOS")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_lock_lock)
                .setOngoing(true)
                .build()
        } else {
            @Suppress("DEPRECATION")
            Notification.Builder(this)
                .setContentTitle("VOS")
                .setContentText(text)
                .setSmallIcon(android.R.drawable.ic_lock_lock)
                .setOngoing(true)
                .build()
        }
    }
}
