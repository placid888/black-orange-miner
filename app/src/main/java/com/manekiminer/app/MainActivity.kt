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
import android.os.Build
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
    external fun setMiningIntensity(isFullSpeed: Boolean)

    companion object {
        init {
            System.loadLibrary("manekiminer")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val rootLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.parseColor("#000000"))
        }

        val topLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(32, 48, 32, 16)
        }

        tvTime = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 42f)
            gravity = Gravity.CENTER
        }

        tvDate = TextView(this).apply {
            setTextColor(Color.parseColor("#CCCCCC"))
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

        val animationContainer = FrameLayout(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                0,
                1.0f 
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

        val bottomCardLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(32, 48, 32, 48)
            
            val cardBackground = GradientDrawable().apply {
                setColor(Color.parseColor("#1A1A1A")) 
                cornerRadius = 48f 
                setStroke(3, Color.parseColor("#33FFA500")) 
            }
            background = cardBackground
            
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                setMargins(48, 16, 48, 64) 
            }
        }

        tvMode = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 16)
        }

        tvMiningStatus = TextView(this).apply {
            setTextColor(Color.parseColor("#00E676"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            typeface = Typeface.MONOSPACE
            gravity = Gravity.CENTER
        }

        bottomCardLayout.addView(tvMode)
        bottomCardLayout.addView(tvMiningStatus)

        rootLayout.addView(topLayout)
        rootLayout.addView(animationContainer)
        rootLayout.addView(bottomCardLayout)

        setContentView(rootLayout)
        tvMiningStatus.text = stringFromJNI()

        // 新增：啟動背景防護服務
        val serviceIntent = Intent(this, MiningService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent)
        } else {
            startService(serviceIntent)
        }

        startClock()
        fetchWeather()
        registerBatteryReceiver()
    }

    private fun switchCatMode(isPluggedIn: Boolean) {
        catLottieView.cancelAnimation()
        
        if (isPluggedIn) {
            catLottieView.setAnimation("orange_cat.json")
            catLottieView.scaleX = 1.2f 
            catLottieView.scaleY = 1.2f
            catLottieView.speed = 1.5f  
        } else {
            catLottieView.setAnimation("black_cat.json")
            catLottieView.scaleX = 0.85f 
            catLottieView.scaleY = 0.85f
            catLottieView.speed = 0.6f   
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
                runOnUiThread { tvWeather.text = "氣溫: $temperature°C | 風速: $windSpeed km/h" }
            } catch (e: Exception) {
                e.printStackTrace()
                runOnUiThread { tvWeather.text = "氣象資訊獲取失敗" }
            }
        }
    }

    private val powerReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                Intent.ACTION_POWER_CONNECTED -> {
                    tvMode.text = "【小菊模式】電源已連接\n全速運算中"
                    switchCatMode(true)
                    setMiningIntensity(true)
                }
                Intent.ACTION_POWER_DISCONNECTED -> {
                    tvMode.text = "【小月模式】電池供電中\n低功耗運算"
                    switchCatMode(false)
                    setMiningIntensity(false)
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
            tvMode.text = "【小菊模式】電源已連接\n全速運算中"
            switchCatMode(true)
            if (!isMining) {
                isMining = true
                startMiningNative()
            }
            setMiningIntensity(true)
        } else {
            tvMode.text = "【小月模式】電池供電中\n低功耗運算"
            switchCatMode(false)
            if (!isMining) {
                isMining = true
                startMiningNative()
            }
            setMiningIntensity(false)
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
                val shortHash = if (hash.length > 16) "${hash.take(8)}...${hash.takeLast(8)}" else hash
                tvMiningStatus.text = "$status\nNonce: $nonce\nHash: $shortHash"
            }
        }
    }
}