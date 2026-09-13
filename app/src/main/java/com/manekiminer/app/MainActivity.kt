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
import android.widget.Button
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
    private lateinit var tvBatteryTemp: TextView // 新增：顯示電池溫度
    private lateinit var tvMode: TextView
    private lateinit var tvMiningStatus: TextView
    private lateinit var catLottieView: LottieAnimationView

    private val handler = Handler(Looper.getMainLooper())
    private var isMining = false

    // 新增：狀態追蹤，防止廣播頻繁觸發導致動畫不斷重啟
    private enum class MiningMode { FULL_SPEED, LOW_POWER, THERMAL_THROTTLE }
    private var currentMode: MiningMode? = null
    private var isPluggedIn = false
    private var batteryTemp = 0.0f

    external fun stringFromJNI(): String
    external fun startMiningNative()
    external fun setMiningIntensity(isFullSpeed: Boolean)
    external fun stopMiningNative() // 新增：C++ 的停止開關

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

        // 新增：電池溫度 UI 元件
        tvBatteryTemp = TextView(this).apply {
            setTextColor(Color.parseColor("#4CAF50")) // 預設綠色
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            gravity = Gravity.CENTER
            setPadding(0, 4, 0, 0)
        }

        topLayout.addView(tvTime)
        topLayout.addView(tvDate)
        topLayout.addView(tvWeather)
        topLayout.addView(tvBatteryTemp)

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
                setMargins(48, 16, 48, 32) 
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

        // 新增：安全退出按鈕
        val btnExit = Button(this).apply {
            text = "停止挖礦並退出系統"
            setTextColor(Color.WHITE)
            typeface = Typeface.DEFAULT_BOLD
            
            background = GradientDrawable().apply {
                setColor(Color.parseColor("#D32F2F")) // 暗紅色
                cornerRadius = 24f
            }
            
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                setMargins(48, 0, 48, 48)
            }
            
            setOnClickListener {
                executeGracefulShutdown()
            }
        }

        rootLayout.addView(topLayout)
        rootLayout.addView(animationContainer)
        rootLayout.addView(bottomCardLayout)
        rootLayout.addView(btnExit) // 將按鈕加入底部

        setContentView(rootLayout)
        tvMiningStatus.text = stringFromJNI()

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

    // 新增：整合退出邏輯
    private fun executeGracefulShutdown() {
        tvMiningStatus.text = "系統安全關閉中..."
        
        // 1. 中斷 C++ 底層運算迴圈與 Socket
        stopMiningNative()
        
        // 2. 通知 Foreground Service 釋放 WakeLock 並自我了斷
        val stopIntent = Intent(this, MiningService::class.java).apply {
            action = MiningService.ACTION_STOP_SERVICE
        }
        startService(stopIntent)
        
        // 3. 延遲 500ms 確保資源釋放後，徹底移除多工任務
        handler.postDelayed({
            finishAndRemoveTask()
        }, 500)
    }

    // 核心邏輯：狀態機，避免頻繁呼叫動畫重置
    private fun updateMiningMode() {
        // 更新溫度 UI
        tvBatteryTemp.text = "電池溫度: ${batteryTemp}°C"
        if (batteryTemp >= 40.0f) {
            tvBatteryTemp.setTextColor(Color.parseColor("#FF5252")) // 紅色警告
        } else {
            tvBatteryTemp.setTextColor(Color.parseColor("#4CAF50")) // 正常綠色
        }

        // 判斷下一個狀態
        val targetMode = if (batteryTemp >= 40.0f) {
            MiningMode.THERMAL_THROTTLE
        } else if (isPluggedIn) {
            MiningMode.FULL_SPEED
        } else {
            MiningMode.LOW_POWER
        }

        // 如果狀態沒有改變，則不執行後續更新，避免動畫閃爍
        if (currentMode == targetMode) return
        currentMode = targetMode

        when (targetMode) {
            MiningMode.THERMAL_THROTTLE -> {
                tvMode.text = "【高溫保護】電池過熱\n強制低功耗黑貓模式"
                tvMode.setTextColor(Color.parseColor("#FF5252"))
                switchCatMode(false)
                setMiningIntensity(false)
            }
            MiningMode.FULL_SPEED -> {
                tvMode.text = "【小菊模式】電源已連接\n全速運算中"
                tvMode.setTextColor(Color.parseColor("#FFA500"))
                switchCatMode(true)
                setMiningIntensity(true)
            }
            MiningMode.LOW_POWER -> {
                tvMode.text = "【小月模式】電池供電中\n低功耗運算"
                tvMode.setTextColor(Color.parseColor("#FFA500"))
                switchCatMode(false)
                setMiningIntensity(false)
            }
        }
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

    // 修改：同時監聽電源狀態與電池變化(包含溫度)
    private val systemReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                Intent.ACTION_POWER_CONNECTED -> {
                    isPluggedIn = true
                    updateMiningMode()
                }
                Intent.ACTION_POWER_DISCONNECTED -> {
                    isPluggedIn = false
                    updateMiningMode()
                }
                Intent.ACTION_BATTERY_CHANGED -> {
                    // 電池溫度單位是 0.1 度，所以要除以 10
                    val tempInt = intent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0)
                    batteryTemp = tempInt / 10.0f
                    updateMiningMode()
                }
            }
        }
    }

    private fun registerBatteryReceiver() {
        val filter = IntentFilter().apply {
            addAction(Intent.ACTION_POWER_CONNECTED)
            addAction(Intent.ACTION_POWER_DISCONNECTED)
            addAction(Intent.ACTION_BATTERY_CHANGED) // 註冊電池變更廣播抓溫度
        }
        registerReceiver(systemReceiver, filter)
        
        // 獲取初始狀態
        val batteryStatus: Intent? = registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val status: Int = batteryStatus?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        val tempInt: Int = batteryStatus?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) ?: 0
        
        isPluggedIn = status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL
        batteryTemp = tempInt / 10.0f
        
        updateMiningMode() // 初始化 UI 狀態
        
        if (!isMining) {
            isMining = true
            startMiningNative()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(systemReceiver)
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