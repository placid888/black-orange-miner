package com.manekiminer.app

import android.animation.ObjectAnimator
import android.animation.ValueAnimator
import android.app.Activity
import android.app.AlertDialog
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.media.MediaPlayer
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
import android.widget.ScrollView
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
import java.util.LinkedList
import kotlin.concurrent.thread

class MainActivity : Activity() {

    private lateinit var tvTime: TextView
    private lateinit var tvDate: TextView
    private lateinit var tvWeather: TextView
    private lateinit var tvBtcPrice: TextView
    private lateinit var tvBatteryTemp: TextView
    private lateinit var tvScreenToggle: TextView
    private lateinit var tvMode: TextView
    private lateinit var catLottieView: LottieAnimationView

    private lateinit var valStatus: TextView
    private lateinit var valUptime: TextView
    private lateinit var valHashrate: TextView
    private lateinit var valJobId: TextView
    private lateinit var valDifficulty: TextView
    private lateinit var valRoundTime: TextView     
    private lateinit var valPrevRoundTime: TextView 
    private lateinit var valShares: TextView
    private lateinit var valHash: TextView
    private lateinit var valNonce: TextView
    
    private lateinit var svTerminal: ScrollView
    private lateinit var tvTerminal: TextView
    private val hashLog = LinkedList<String>()

    private val handler = Handler(Looper.getMainLooper())
    private var isMining = false
    private var isScreenKeptOn = true

    private enum class MiningMode { FULL_SPEED, LOW_POWER, THERMAL_THROTTLE }
    private var currentMode: MiningMode? = null
    private var isPluggedIn = false
    private var batteryTemp = 0.0f
    
    private var borderAnimator: ValueAnimator? = null
    private lateinit var dashboardDrawable: GradientDrawable

    external fun stringFromJNI(): String
    external fun startMiningNative()
    external fun setMiningIntensity(isFullSpeed: Boolean)
    external fun stopMiningNative()

    companion object {
        init {
            System.loadLibrary("manekiminer")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

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

        tvBtcPrice = TextView(this).apply {
            setTextColor(Color.parseColor("#F7931A"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            typeface = Typeface.MONOSPACE
            gravity = Gravity.CENTER
            setPadding(0, 6, 0, 2)
            text = "₿ BTC: 連線更新中..."
        }

        tvBatteryTemp = TextView(this).apply {
            setTextColor(Color.parseColor("#4CAF50"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            gravity = Gravity.CENTER
            setPadding(0, 4, 0, 0)
        }

        tvScreenToggle = TextView(this).apply {
            text = "💡 螢幕常亮: 🟢開啟 (點擊切換)"
            setTextColor(Color.parseColor("#4DB6AC"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 14f)
            gravity = Gravity.CENTER
            setPadding(0, 16, 0, 0)
            isClickable = true
            
            setOnClickListener {
                isScreenKeptOn = !isScreenKeptOn
                if (isScreenKeptOn) {
                    text = "💡 螢幕常亮: 🟢開啟 (點擊切換)"
                    setTextColor(Color.parseColor("#4DB6AC"))
                    window.addFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                } else {
                    text = "💡 螢幕常亮: 🔴關閉 (允許休眠)"
                    setTextColor(Color.parseColor("#757575"))
                    window.clearFlags(android.view.WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                }
            }
        }

        topLayout.addView(tvTime)
        topLayout.addView(tvDate)
        topLayout.addView(tvWeather)
        topLayout.addView(tvBtcPrice)
        topLayout.addView(tvBatteryTemp)
        topLayout.addView(tvScreenToggle)

        val animationContainer = FrameLayout(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1.0f 
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

        dashboardDrawable = GradientDrawable().apply {
            setColor(Color.parseColor("#121212")) 
            cornerRadius = 32f 
            setStroke(3, Color.parseColor("#33FFFFFF")) 
        }

        val bottomCardLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(48, 32, 48, 48)
            background = dashboardDrawable
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                setMargins(48, 16, 48, 16) 
            }
        }

        tvMode = TextView(this).apply {
            setTextColor(Color.parseColor("#FFA500"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 15f)
            typeface = Typeface.DEFAULT_BOLD
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 16)
        }
        bottomCardLayout.addView(tvMode)

        valStatus = createDashboardRow(bottomCardLayout, "節點狀態", "#00E676")
        valUptime = createDashboardRow(bottomCardLayout, "運行時間", "#E0E0E0")
        valHashrate = createDashboardRow(bottomCardLayout, "即時算力", "#00B0FF")
        valJobId = createDashboardRow(bottomCardLayout, "當前任務", "#FF9800")
        valDifficulty = createDashboardRow(bottomCardLayout, "區塊難度", "#00BCD4")
        valRoundTime = createDashboardRow(bottomCardLayout, "本輪耗時", "#FF4081")     
        valPrevRoundTime = createDashboardRow(bottomCardLayout, "上輪耗時", "#757575") 
        valShares = createDashboardRow(bottomCardLayout, "有效提交", "#FFD600")
        valNonce = createDashboardRow(bottomCardLayout, "隨機雜湊", "#B388FF")
        valHash = createDashboardRow(bottomCardLayout, "當前運算", "#9E9E9E")
        
        svTerminal = ScrollView(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 200
            ).apply { setMargins(48, 0, 48, 32) }
            background = GradientDrawable().apply {
                setColor(Color.parseColor("#080808"))
                cornerRadius = 16f
                setStroke(2, Color.parseColor("#1A00FF00"))
            }
            setPadding(24, 24, 24, 24)
        }
        
        tvTerminal = TextView(this).apply {
            setTextColor(Color.parseColor("#00E676"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 10f)
            typeface = Typeface.MONOSPACE
        }
        svTerminal.addView(tvTerminal)

        val btnExit = Button(this).apply {
            text = "停止挖礦並退出系統"
            setTextColor(Color.WHITE)
            typeface = Typeface.DEFAULT_BOLD
            background = GradientDrawable().apply {
                setColor(Color.parseColor("#D32F2F")) 
                cornerRadius = 24f
            }
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply {
                setMargins(48, 0, 48, 48)
            }
            setOnClickListener { executeGracefulShutdown() }
        }

        rootLayout.addView(topLayout)
        rootLayout.addView(animationContainer)
        rootLayout.addView(bottomCardLayout)
        rootLayout.addView(svTerminal)
        rootLayout.addView(btnExit)

        setContentView(rootLayout)
        valStatus.text = stringFromJNI()

        val serviceIntent = Intent(this, MiningService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent)
        } else {
            startService(serviceIntent)
        }

        startClock()
        fetchWeather()
        startBtcPricePolling()
        registerBatteryReceiver()
    }

    private fun createDashboardRow(parent: LinearLayout, label: String, valueColor: String): TextView {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT
            ).apply { setMargins(0, 6, 0, 6) } 
        }
        
        val tvLabel = TextView(this).apply {
            text = label
            setTextColor(Color.parseColor("#888888"))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
            typeface = Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        }
        
        val tvValue = TextView(this).apply {
            text = "-"
            setTextColor(Color.parseColor(valueColor))
            setTextSize(TypedValue.COMPLEX_UNIT_SP, 13f)
            typeface = Typeface.MONOSPACE
            gravity = Gravity.END
            layoutParams = LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 2.5f)
        }
        
        row.addView(tvLabel)
        row.addView(tvValue)
        parent.addView(row)
        return tvValue
    }

    private fun showJackpotDialog(winningNonce: Int, winningHash: String) {
        try {
            var resourceId = resources.getIdentifier("jackpot_sound", "raw", packageName)
            if (resourceId == 0) {
                resourceId = resources.getIdentifier("background_sound", "raw", packageName)
            }
            if (resourceId != 0) {
                val mp = MediaPlayer.create(this, resourceId)
                mp.setOnCompletionListener { it.release() }
                mp.start()
            }
        } catch (e: Exception) {
            e.printStackTrace()
        }

        val dialogView = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(64, 80, 64, 80)
            background = GradientDrawable().apply {
                colors = intArrayOf(Color.parseColor("#FFD700"), Color.parseColor("#FFA000"))
                cornerRadius = 48f
            }
            addView(TextView(context).apply {
                text = "🎉 JACKPOT! 貓咪發威啦! 🎉\n成功找到有效區塊！"
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 24f)
                setTextColor(Color.parseColor("#B71C1C"))
                typeface = Typeface.DEFAULT_BOLD
                gravity = Gravity.CENTER
                setPadding(0, 0, 0, 32)
            })
            addView(TextView(context).apply {
                text = String.format("神聖 Nonce: 0x%08X", winningNonce)
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 16f)
                setTextColor(Color.BLACK)
                typeface = Typeface.MONOSPACE
                gravity = Gravity.CENTER
            })
            addView(TextView(context).apply {
                text = "Winning Hash:\n$winningHash"
                setTextSize(TypedValue.COMPLEX_UNIT_SP, 12f)
                setTextColor(Color.parseColor("#333333"))
                typeface = Typeface.MONOSPACE
                gravity = Gravity.CENTER
                setPadding(0, 16, 0, 0)
            })
        }

        AlertDialog.Builder(this)
            .setView(dialogView)
            .setCancelable(false) 
            .setPositiveButton("太神啦！繼續挖") { dialog, _ -> dialog.dismiss() }
            .show()
    }

    fun updateMiningStatus(status: String, nonce: Int, hash: String, hashrate: Double, shares: Int, uptime: Long, jobId: String, difficulty: String, roundTime: Long, prevRoundTime: Long) {
        runOnUiThread {
            valStatus.text = status
            
            if (status.contains("🎯")) showJackpotDialog(nonce, hash)
            
            if (uptime > 0) {
                val hours = uptime / 3600
                val minutes = (uptime % 3600) / 60
                val seconds = uptime % 60
                valUptime.text = String.format(Locale.US, "%02d:%02d:%02d", hours, minutes, seconds)
            }

            if (jobId != "-") valJobId.text = jobId
            if (difficulty != "-") valDifficulty.text = "0x$difficulty"

            val formatTime = { timeSec: Long ->
                if (timeSec == 0L) "-"
                else String.format(Locale.US, "%02d:%02d", timeSec / 60, timeSec % 60)
            }
            
            valRoundTime.text = formatTime(roundTime)
            valPrevRoundTime.text = formatTime(prevRoundTime)

            valHashrate.text = when {
                hashrate >= 1_000_000 -> String.format(Locale.US, "%.2f MH/s", hashrate / 1_000_000)
                hashrate >= 1_000 -> String.format(Locale.US, "%.2f kH/s", hashrate / 1_000)
                else -> String.format(Locale.US, "%.2f H/s", hashrate)
            }
            
            valShares.text = "$shares Shares"
            
            if (nonce != 0 || hash.isNotEmpty()) {
                val shortHash = if (hash.length > 16) "${hash.take(8)}...${hash.takeLast(8)}" else hash
                valNonce.text = String.format("0x%08X", nonce) 
                valHash.text = shortHash
                
                val logLine = String.format("0x%08X > %s", nonce, hash.take(24) + "...")
                hashLog.add(logLine)
                if (hashLog.size > 8) hashLog.removeFirst()
                tvTerminal.text = hashLog.joinToString("\n")
                svTerminal.post { svTerminal.fullScroll(ScrollView.FOCUS_DOWN) }
            }
        }
    }

    private fun executeGracefulShutdown() {
        valStatus.text = "系統安全關閉中..."
        stopMiningNative()
        
        val stopIntent = Intent(this, MiningService::class.java).apply {
            action = MiningService.ACTION_STOP_SERVICE
        }
        startService(stopIntent)
        
        handler.postDelayed({ finishAndRemoveTask() }, 500)
    }

    private fun updateMiningMode() {
        tvBatteryTemp.text = "電池溫度: ${batteryTemp}°C"
        if (batteryTemp >= 40.0f) {
            tvBatteryTemp.setTextColor(Color.parseColor("#FF5252")) 
        } else {
            tvBatteryTemp.setTextColor(Color.parseColor("#4CAF50")) 
        }

        val targetMode = if (batteryTemp >= 40.0f) {
            MiningMode.THERMAL_THROTTLE
        } else if (isPluggedIn) {
            MiningMode.FULL_SPEED
        } else {
            MiningMode.LOW_POWER
        }

        if (currentMode == targetMode) return
        currentMode = targetMode

        when (targetMode) {
            MiningMode.THERMAL_THROTTLE -> {
                tvMode.text = "【高溫保護】強制低功耗黑貓模式"
                tvMode.setTextColor(Color.parseColor("#FF5252"))
                switchCatMode(false)
                setMiningIntensity(false)
                stopBorderAnimation()
            }
            MiningMode.FULL_SPEED -> {
                tvMode.text = "【小菊模式】電源連接全速運算"
                tvMode.setTextColor(Color.parseColor("#FFA500"))
                switchCatMode(true)
                setMiningIntensity(true)
                startBorderAnimation()
            }
            MiningMode.LOW_POWER -> {
                tvMode.text = "【小月模式】電池供電節能運算"
                tvMode.setTextColor(Color.parseColor("#FFA500"))
                switchCatMode(false)
                setMiningIntensity(false)
                stopBorderAnimation()
            }
        }
    }

    private fun startBorderAnimation() {
        if (borderAnimator != null && borderAnimator!!.isRunning) return
        
        borderAnimator = ValueAnimator.ofArgb(Color.parseColor("#33FFFFFF"), Color.parseColor("#FF9800")).apply {
            duration = 300
            repeatMode = ValueAnimator.REVERSE
            repeatCount = ValueAnimator.INFINITE
            addUpdateListener { animator ->
                dashboardDrawable.setStroke(4, animator.animatedValue as Int)
            }
            start()
        }
    }

    private fun stopBorderAnimation() {
        borderAnimator?.cancel()
        dashboardDrawable.setStroke(3, Color.parseColor("#33FFFFFF"))
    }

    private fun switchCatMode(isPluggedIn: Boolean) {
        catLottieView.cancelAnimation()
        if (isPluggedIn) {
            catLottieView.setAnimation("orange_cat.json")
            catLottieView.scaleX = 1.3f 
            catLottieView.scaleY = 1.3f
            catLottieView.speed = 2.5f
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

    private fun startBtcPricePolling() {
        handler.post(object : Runnable {
            override fun run() {
                fetchBtcPrice()
                handler.postDelayed(this, 60000)
            }
        })
    }

    private fun fetchBtcPrice() {
        thread {
            try {
                val url = URL("https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=twd")
                val connection = url.openConnection() as HttpURLConnection
                connection.requestMethod = "GET"
                connection.connectTimeout = 5000
                connection.readTimeout = 5000
                connection.setRequestProperty("User-Agent", "Mozilla/5.0")
                val reader = BufferedReader(InputStreamReader(connection.inputStream))
                val response = reader.readText()
                reader.close()
                
                val jsonObject = JSONObject(response)
                val btcTwd = jsonObject.getJSONObject("bitcoin").getDouble("twd")
                val blockRewardTwd = btcTwd * 3.125

                runOnUiThread {
                    tvBtcPrice.text = String.format(Locale.TAIWAN, "₿ NT$%,.0f | 爆塊: NT$%,.0f", btcTwd, blockRewardTwd)
                }
            } catch (e: Exception) {
                e.printStackTrace()
                runOnUiThread {
                    tvBtcPrice.text = "₿ BTC 行情更新失敗"
                }
            }
        }
    }

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
            addAction(Intent.ACTION_BATTERY_CHANGED) 
        }
        registerReceiver(systemReceiver, filter)
        
        val batteryStatus: Intent? = registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val status: Int = batteryStatus?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        val tempInt: Int = batteryStatus?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) ?: 0
        
        isPluggedIn = status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL
        batteryTemp = tempInt / 10.0f
        
        updateMiningMode() 
        
        if (!isMining) {
            isMining = true
            startMiningNative()
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        unregisterReceiver(systemReceiver)
        borderAnimator?.cancel()
    }
}