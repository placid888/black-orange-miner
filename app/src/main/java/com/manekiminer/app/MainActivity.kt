package com.manekiminer.app

import android.app.Activity
import android.graphics.Color
import android.os.Bundle
import android.view.Gravity
import android.widget.LinearLayout
import android.widget.TextClock
import android.widget.TextView

class MainActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 建立主排版容器 (垂直排列，純黑背景，內容置中)
        val mainLayout = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setBackgroundColor(Color.BLACK)
        }

        // 建立動態時鐘元件 (亮橘色，24小時制)
        val textClock = TextClock(this).apply {
            format24Hour = "HH:mm:ss"
            format12Hour = null
            setTextColor(Color.parseColor("#FFA500"))
            textSize = 80f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 40)
        }

        // 建立日期元件 (亮橘色)
        val dateClock = TextClock(this).apply {
            format24Hour = "yyyy-MM-dd EEEE"
            format12Hour = null
            setTextColor(Color.parseColor("#FFA500"))
            textSize = 24f
            gravity = Gravity.CENTER
            setPadding(0, 0, 0, 80)
        }

        // 建立底層狀態文字視圖 (暗橘色，顯示 C++ 引擎狀態)
        val statusView = TextView(this).apply {
            text = stringFromJNI()
            setTextColor(Color.parseColor("#CC8400"))
            textSize = 18f
            gravity = Gravity.CENTER
        }

        // 將元件依序加入主排版
        mainLayout.addView(textClock)
        mainLayout.addView(dateClock)
        mainLayout.addView(statusView)

        setContentView(mainLayout)
    }

    external fun stringFromJNI(): String

    companion object {
        init {
            System.loadLibrary("manekiminer")
        }
    }
}