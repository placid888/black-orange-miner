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
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import kotlin.concurrent.thread

class MainActivity : Activity() {

    private lateinit var statusView: TextView
    private lateinit var weatherView: TextView
    private lateinit var powerReceiver: BroadcastReceiver

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
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
            setPadding(0, 0, 0, 20)
        }

        val dateClock = TextClock(this).apply {
            format24Hour = "yyyy-MM-dd EEEE"
            format12Hour = null
            setTextColor(Color.parseColor("#FFA500"))
            textSize = 24f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 20)
        }

        weatherView = TextView(this).apply {
            text = "氣象資料載入中..."
            setTextColor(Color.parseColor("#FFA500"))
            textSize = 24f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 60)
        }

        statusView = TextView(this).apply {
            text = "系統初始化中..."
            setTextColor(Color.parseColor("#CC8400"))
            textSize = 18f
            gravity = Gravity.CENTER
        }

        mainLayout.addView(textClock)
        mainLayout.addView(dateClock)
        mainLayout.addView(weatherView)
        mainLayout.addView(statusView)

        setContentView(mainLayout)
        
        setupPowerReceiver()
        fetchWeatherData()
        startMiningNative()
    }

    private fun fetchWeatherData() {
        thread {
            try {
                val url = URL("https://api.open-meteo.com/v1/forecast?latitude=24.90&longitude=121.04&current_weather=true")
                val connection = url.openConnection() as HttpURLConnection
                connection.requestMethod = "GET"
                connection.connectTimeout = 5000
                
                if (connection.responseCode == HttpURLConnection.HTTP_OK) {
                    val reader = BufferedReader(InputStreamReader(connection.inputStream))
                    val response = reader.readText()
                    reader.close()
                    
                    val jsonObject = JSONObject(response)
                    val currentWeather = jsonObject.getJSONObject("current_weather")
                    val temp = currentWeather.getDouble("temperature")
                    val windSpeed = currentWeather.getDouble("windspeed")
                    
                    runOnUiThread {
                        weatherView.text = "氣溫: $temp°C | 風速: $windSpeed km/h"
                    }
                }
            } catch (e: Exception) {
                runOnUiThread {
                    weatherView.text = "氣象資料更新失敗"
                }
            }
        }
    }

    private fun setupPowerReceiver() {
        powerReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                val status = intent?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
                val isCharging = status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL
                
                if (isCharging) {
                    statusView.text = "【小菊模式】電源已連接，允許全速運算\n${stringFromJNI()}"
                } else {
                    statusView.text = "【小月模式】待機監視中，暫停運算\n${stringFromJNI()}"
                }
            }
        }
        registerReceiver(powerReceiver, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(powerReceiver)
    }

    external fun stringFromJNI(): String
    external fun startMiningNative()

    companion object {
        init {
            System.loadLibrary("manekiminer")
        }
    }
}