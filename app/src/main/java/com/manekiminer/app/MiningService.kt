package com.manekiminer.app

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.IBinder
import android.os.PowerManager

class MiningService : Service() {
    private var wakeLock: PowerManager.WakeLock? = null

    override fun onCreate() {
        super.onCreate()
        acquireWakeLock()
        startForegroundService()
    }

    private fun startForegroundService() {
        val channelId = "MiningServiceChannel"
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId,
                "挖礦背景服務",
                NotificationManager.IMPORTANCE_LOW
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(channel)
        }

        val notification: Notification = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Notification.Builder(this, channelId)
                .setContentTitle("ManekiMiner 運行中")
                .setContentText("貓咪正在背景為您努力尋找區塊...")
                .setSmallIcon(android.R.drawable.ic_menu_preferences) 
                .build()
        } else {
            Notification.Builder(this)
                .setContentTitle("ManekiMiner 運行中")
                .setContentText("貓咪正在背景為您努力尋找區塊...")
                .setSmallIcon(android.R.drawable.ic_menu_preferences)
                .build()
        }

        startForeground(1, notification)
    }

    private fun acquireWakeLock() {
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(
            PowerManager.PARTIAL_WAKE_LOCK,
            "ManekiMiner::BackgroundWakeLock"
        )
        wakeLock?.acquire() 
    }

    override fun onDestroy() {
        super.onDestroy()
        wakeLock?.let {
            if (it.isHeld) {
                it.release()
            }
        }
    }

    override fun onBind(intent: Intent?): IBinder? {
        return null
    }
}