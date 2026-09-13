package com.manekiminer.app

import android.animation.ObjectAnimator
import android.app.Activity
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.BatteryManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.TypedValue
import android.view.Gravity
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import com.airbnb.lottie.LottieAnimationView
import com.airbnb.lottie.LottieDrawable
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
    private lateinit var catLottieView: LottieAnimationView

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

        // 1. 根佈局 (深灰底色)
        val rootLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#000000")) // 較柔和的深灰色
        }

        // 2. 頂部狀態列
        val topLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(32, 48, 32, 16)
        }

        tvTime = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 42f) // 縮小時間字體
            gravity = Gravity.CENTER
        }

        tvDate = TextView(this).apply {
            setTextColor(Color.parseColor("#CCCCCC")) // 改為淺灰增加層次
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            gravity = Gravity.CENTER
            setPadding(0, 8, 0, 8)
        }

        tvWeather = TextView(this).apply {
            setTextColor(Color.parseColor("#AAAAAA"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            gravity = Gravity.CENTER
        }

        topLayout.addView(tvTime)
        topLayout.addView(tvDate)
        topLayout.addView(tvWeather)

        // 3. 中央動畫舞台 (使用 Weight 自動撐開空間)
        val animationContainer = FrameLayout(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0,
                1.0f // Weight = 1
            )
        }

        catLottieView = LottieAnimationView(this).apply {
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            ).apply {
                gravity = Gravity.CENTER
                setMargins(32, 32, 32, 32)
            }
            repeatCount = LottieDrawable.INFINITE
        }
        animationContainer.addView(catLottieView)

        // 4. 底部數據卡片
        val bottomCardLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(32, 48, 32, 48)
            
            // 設定半透明圓角背景
            val cardBackground = GradientDrawable().apply {
                setColor(Color.parseColor("#1A1A1A")) 
                cornerRadius = 48f 
                setStroke(3, Color.parseColor("#33FFA500")) // 微透明橘色邊框
            }
            background = cardBackground
            
            val params = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                setMargins(48, 16, 48, 64) // 卡片外部邊距
            }
            layoutParams = params
        }

        tvMode = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 16)
        }

        tvMiningStatus = TextView(this).apply {
            setTextColor(Color.parseColor("#00E676")) // 挖礦數據改為科技感亮綠色
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            typeface = Typeface.MONOSPACE // 使用等寬字體對齊 Hash 值
            gravity = Gravity.CENTER
        }

        bottomCardLayout.addView(tvMode)
        bottomCardLayout.addView(tvMiningStatus)

        // 將三大區塊加入根佈局
        rootLayout.addView(topLayout)
        rootLayout.addView(animationContainer)
        rootLayout.addView(bottomCardLayout)

        setContentView(rootLayout)

        tvMiningStatus.text = stringFromJNI()

        startClock()
        fetchWeather()
        registerBatteryReceiver()
    }

    private fun switchCatMode(isPluggedIn: Boolean) {
        catLottieView.cancelAnimation()
        
        if (isPluggedIn) {
            catLottieView.setAnimation("orange_cat.json")
            catLottieView.scaleX = 1.2f // 小菊比較胖，放大
            catLottieView.scaleY = 1.2f
            catLottieView.speed = 1.5f  // 全速運算，動畫加快
        } else {
            catLottieView.setAnimation("black_cat.json")
            catLottieView.scaleX = 0.85f // 小月瘦小，縮小
            catLottieView.scaleY = 0.85f
            catLottieView.speed = 0.6f   // 待機模式，動作放緩
        }
        
        catLottieView.alpha = 0f
        catLottieView.playAnimation()
        
        ObjectAnimator.ofFloat(catLottieView, "alpha", 0f, 1f).apply {
            duration = 600
            start()
        }
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
                    tvMode.text = "【小菊模式】電源已連接\n允許全速運算"
                    switchCatMode(true)
                    if (!isMining) {
                        isMining = true
                        startMiningNative()
                    }
                }
                Intent.ACTION_POWER_DISCONNECTED -> {
                    tvMode.text = "【小月模式】待機監視中\n暫停運算"
                    switchCatMode(false)
                    isMining = false
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
            tvMode.text = "【小菊模式】電源已連接\n允許全速運算"
            switchCatMode(true)
            if (!isMining) {
                isMining = true
                startMiningNative()
            }
        } else {
            tvMode.text = "【小月模式】待機監視中\n暫停運算"
            switchCatMode(false)
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
                // 將過長的 Hash 截斷顯示前後8碼，中間用 ... 縮略，保持介面簡潔
                val shortHash = if (hash.length > 16) "${hash.take(8)}...${hash.takeLast(8)}" else hash
                tvMiningStatus.text = "$status\nNonce: $nonce\nHash: $shortHash"
            }
        }
    }
}