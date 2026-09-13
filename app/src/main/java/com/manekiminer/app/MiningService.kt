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

    // 宣告一個常數，讓外部可以呼叫關閉
    companion object {
        const val ACTION_STOP_SERVICE = "STOP_MINING_SERVICE"
    }

    override fun onCreate() {
        super.onCreate()
        acquireWakeLock()
        startForegroundService()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // 接收到退出指令時，自我了斷
        if (intent?.action == ACTION_STOP_SERVICE) {
            stopSelf()
            return START_NOT_STICKY
        }
        
        // START_STICKY：如果被系統因記憶體不足而殺死，系統會嘗試重新啟動服務
        return START_STICKY 
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
        // 加上一個超時保護 (例如 24 小時 = 24 * 60 * 60 * 1000L)，防止極端崩潰死鎖
        wakeLock?.acquire(86400000L) 
    }

    override fun onDestroy() {
        // 1. 釋放 WakeLock
        wakeLock?.let {
            if (it.isHeld) {
                it.release()
            }
        }
        
        // 2. 徹底移除狀態列通知
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            stopForeground(STOP_FOREGROUND_REMOVE)
        } else {
            @Suppress("DEPRECATION")
            stopForeground(true)
        }

        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? {
        return null
    }
}