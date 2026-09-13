package com.manekiminer.app

import android.app.Activity
import android.os.Bundle
import android.widget.TextView
import android.graphics.Color
import android.view.Gravity

class MainActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // 動態建立一個文字視圖，以純黑底色與橘色文字顯示，進行防烙印與主題測試
        val textView = TextView(this).apply {
            text = stringFromJNI()
            setTextColor(Color.parseColor("#FFA500")) // 橘色字體
            setBackgroundColor(Color.BLACK)           // 純黑背景
            textSize = 24f
            gravity = Gravity.CENTER
        }
        
        setContentView(textView)
    }

    // 宣告外部 C++ 函式
    external fun stringFromJNI(): String

    companion object {
        // 在應用程式啟動時，載入名為 manekiminer 的 C++ 動態連結庫
        init {
            System.loadLibrary("manekiminer")
        }
    }
}