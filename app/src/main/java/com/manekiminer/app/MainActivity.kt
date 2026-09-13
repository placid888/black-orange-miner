package com.manekiminer.app

import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Color
import android.os.BatteryManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.TextView
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.concurrent.thread

class MainActivity : Activity() {

    private lateinit var tvTime: TextView
    private lateinit var tvDate: TextView
    private lateinit var tvWeather: TextView
    private lateinit var tvMode: TextView
    private lateinit var tvMiningStatus: TextView

    private val handler = Handler(Looper.getMainLooper())
    private var isMining = false

    external fun stringFromJNI(): String
    external fun startMiningNative()

    companion object {
        init {
            System.loadLibrary("manekiminer")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val layout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setBackgroundColor(Color.BLACK)
            setPadding(32, 32, 32, 32)
        }

        tvTime = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 56f)
            gravity = Gravity.CENTER
        }

        tvDate = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 18f)
            gravity = Gravity.CENTER
            setPadding(0, 16, 0, 16)
        }

        tvWeather = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 32)
        }

        tvMode = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            gravity = Gravity.CENTER
        }

        tvMiningStatus = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            gravity = Gravity.CENTER
            setPadding(0, 8, 0, 0)
        }

        layout.addView(tvTime)
        layout.addView(tvDate)
        layout.addView(tvWeather)
        layout.addView(tvMode)
        layout.addView(tvMiningStatus)

        setContentView(layout)

        tvMiningStatus.text = stringFromJNI()

        startClock()
        fetchWeather()
        registerBatteryReceiver()
    }

    private fun startClock() {
        handler.post(object : Runnable {
            override fun run() {
                val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
                val dateFormat = SimpleDateFormat("yyyy-MM-dd EEEE", Locale.TAIWAN)
                val now = Date()
                tvTime.text = timeFormat.format(now)
                tvDate.text = dateFormat.format(now)
                handler.postDelayed(this, 1000)
            }
        })
    }

    private fun fetchWeather() {
        thread {
            try {
                val url = URL("https://api.open-meteo.com/v1/forecast?latitude=24.9038&longitude=121.0436&current_weather=true")
                val connection = url.openConnection() as HttpURLConnection
                connection.requestMethod = "GET"
                connection.connectTimeout = 5000
                connection.readTimeout = 5000

                val reader = BufferedReader(InputStreamReader(connection.inputStream))
                val response = reader.readText()
                reader.close()

                val jsonObject = JSONObject(response)
                val currentWeather = jsonObject.getJSONObject("current_weather")
                val temperature = currentWeather.getDouble("temperature")
                val windSpeed = currentWeather.getDouble("windspeed")

                runOnUiThread {
                    tvWeather.text = "氣溫: $temperature°C | 風速: $windSpeed km/h"
                }
            } catch (e: Exception) {
                e.printStackTrace()
                runOnUiThread {
                    tvWeather.text = "氣象資訊獲取失敗"
                }
            }
        }
    }

    private val powerReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                Intent.ACTION_POWER_CONNECTED -> {
                    tvMode.text = "【小菊模式】電源已連接，允許全速運算"
                    if (!isMining) {
                        isMining = true
                        startMiningNative()
                    }
                }
                Intent.ACTION_POWER_DISCONNECTED -> {
                    tvMode.text = "【小月模式】待機監視中，暫停運算"
                }
            }
        }
    }

    private fun registerBatteryReceiver() {
        val filter = IntentFilter().apply {
            addAction(Intent.ACTION_POWER_CONNECTED)
            addAction(Intent.ACTION_POWER_DISCONNECTED)
        }
        registerReceiver(powerReceiver, filter)

        val batteryStatus: Intent? = IntentFilter(Intent.ACTION_BATTERY_CHANGED).let { ifilter ->
            registerReceiver(null, ifilter)
        }
        val status: Int = batteryStatus?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        val isCharging: Boolean = status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL

        if (isCharging) {
            tvMode.text = "【小菊模式】電源已連接，允許全速運算"
            if (!isMining) {
                isMining = true
                startMiningNative()
            }
        } else {
            tvMode.text = "【小月模式】待機監視中，暫停運算"
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(powerReceiver)
    }

    fun updateMiningStatus(status: String, nonce: Int, hash: String) {
        runOnUiThread {
            if (nonce == 0 && hash.isEmpty()) {
                tvMiningStatus.text = status
            } else {
                tvMiningStatus.text = "$status\nNonce: $nonce\nHash: $hash"
            }
        }
    }
}