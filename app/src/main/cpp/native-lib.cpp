#include <jni.h>
#include <string>
#include <thread>
#include <android/log.h>

#define LOG_TAG "ManekiMiner-Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 預備：Stratum 連線參數 (目前先以 Public 測試池為目標)
const std::string POOL_URL = "public-pool.io";
const int POOL_PORT = 21496;
const std::string WALLET_ADDRESS = "bc1qyour_btc_address_here";

void startMiningLoop() {
    LOGI("橘貓全速模式啟動：初始化 Stratum 協定");
    // TODO: 1. 建立 TCP Socket 連線
    // TODO: 2. 傳送 {"id": 1, "method": "mining.subscribe", "params": []}
    // TODO: 3. 傳送 {"id": 2, "method": "mining.authorize", "params": ["WALLET_ADDRESS", "x"]}
    // TODO: 4. 啟動無窮迴圈接收 mining.notify 事件並執行 SHA256(SHA256(BlockHeader))
    
    // 模擬運算迴圈
    int nonce_count = 0;
    while(true) {
        // 暫時模擬運算延遲，避免初期測試即導致手機發熱崩潰
        std::this_thread::sleep_for(std::chrono::seconds(1)); 
        nonce_count++;
        if (nonce_count % 10 == 0) {
            LOGI("已完成 %d 次虛擬雜湊碰撞測試", nonce_count);
        }
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "C++ 挖礦引擎就緒。等待 Stratum 連線指令。";
    return env->NewStringUTF(hello.c_str());
}

// 提供給 Kotlin 觸發背景挖礦的 JNI 介面
extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    // 建立分離執行緒 (Detached Thread)，避免阻塞 Android 介面
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}