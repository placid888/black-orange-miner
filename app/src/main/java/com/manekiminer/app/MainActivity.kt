package com.manekiminer.app

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Color
import android.os.BatteryManager
import android.os.Bundle
import android.view.Gravity
import android.view.WindowManager
import android.widget.LinearLayout
import android.widget.TextClock
import android.widget.TextView

class MainActivity : Activity() {
    external fun stringFromJNI(): String
    external fun startMiningNative() 

    private lateinit var statusView: TextView
    private lateinit var powerReceiver: BroadcastReceiver

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 保持螢幕常亮，防止資訊看板進入休眠
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        val mainLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setBackgroundColor(Color.BLACK)
        }

        val textClock = TextClock(this).apply {
            format24Hour = "HH:mm:ss"
            format12Hour = null
            setTextColor(Color.parseColor("#FFA500"))
            textSize = 80f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 40)
        }

        val dateClock = TextClock(this).apply {
            format24Hour = "yyyy-MM-dd EEEE"
            format12Hour = null
            setTextColor(Color.parseColor("#FFA500"))
            textSize = 24f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 80)
        }

        statusView = TextView(this).apply {
            text = "系統初始化中..."
            setTextColor(Color.parseColor("#CC8400"))
            textSize = 18f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 0)
        }

        mainLayout.addView(textClock)
        mainLayout.addView(dateClock)
        mainLayout.addView(statusView)

        setContentView(mainLayout)
        
        setupPowerReceiver()
    }

    // 監聽電池與充電狀態廣播
    private fun setupPowerReceiver() {
        powerReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                val status = intent?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
                val isCharging = status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL
                
                if (isCharging) {
                    statusView.text = "【橘貓模式】電源已連接，允許全速運算\n${stringFromJNI()}"
                } else {
                    statusView.text = "【黑貓模式】待機監視中，暫停運算\n${stringFromJNI()}"
                }
            }
        }
        // 註冊廣播接收器
        registerReceiver(powerReceiver, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
    }

    override fun onDestroy() {
        super.onDestroy()
        // 釋放廣播接收器避免記憶體洩漏
        unregisterReceiver(powerReceiver)
    }

    external fun stringFromJNI(): String

    companion object {
        init {
            System.loadLibrary("manekiminer")
        }
    }
}