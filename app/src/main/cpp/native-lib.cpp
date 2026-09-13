#include <jni.h>
#include <string>
#include <thread>
#include <android/log.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iomanip>
#include <sstream>
#include "sha256.h" // 引入前一階段建立的 SHA-256 演算法標頭檔

// 定義 Android Logcat 的標籤與輸出巨集，方便在除錯工具中篩選訊息
#define LOG_TAG "ManekiMiner-Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 工具函式：將二進位的位元組陣列 (byte array) 轉換為人類可讀的十六進位字串 (Hex String)
std::string bytesToHexString(const uint8_t* bytes, size_t len) {
    std::stringstream ss;
    for(size_t i = 0; i < len; ++i) {
        // 設定輸出格式：十六進位、寬度為 2、不足補 0
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    }
    return ss.str();
}

// 核心函式：實作比特幣專用的雙重 SHA-256 (Double SHA-256)
// 邏輯：Hash = SHA256(SHA256(BlockHeader))
void double_sha256(const uint8_t* data, size_t len, uint8_t* hash_out) {
    SHA256_CTX ctx;
    uint8_t first_hash[32]; // 存放第一次雜湊結果的緩衝區 (SHA-256 固定輸出 32 bytes)

    // --- 第一層 SHA-256 運算 ---
    sha256_init(&ctx);                  // 初始化運算器狀態
    sha256_update(&ctx, data, len);     // 將原始資料餵給運算器
    sha256_final(&ctx, first_hash);     // 結算並輸出第一次的雜湊值

    // --- 第二層 SHA-256 運算 ---
    sha256_init(&ctx);                  // 再次初始化運算器
    sha256_update(&ctx, first_hash, 32);// 將第一層的結果 (32 bytes) 作為此次的輸入資料
    sha256_final(&ctx, hash_out);       // 結算並輸出最終的雙重雜湊值
}

// 背景挖礦主迴圈
void startMiningLoop() {
    LOGI("小菊全速模式啟動：掛載雙重 SHA-256 引擎");
    
    // 建立一組測試用的字串，模擬礦池派發的區塊標頭 (Block Header)
    const char* test_data = "ManekiMiner_Test_Block";
    size_t data_len = strlen(test_data);
    uint8_t result_hash[32]; // 存放運算結果的陣列

    uint32_t nonce = 0; // 模擬挖礦時不斷遞增的隨機數
    
    while(true) {
        // 暫時加入延遲，避免初期測試時迴圈跑太快導致手機過熱
        std::this_thread::sleep_for(std::chrono::seconds(2)); 
        nonce++;
        
        // 呼叫自訂的雙重雜湊函式進行運算
        double_sha256(reinterpret_cast<const uint8_t*>(test_data), data_len, result_hash);
        
        // 每運算 5 次，將結果印出至 Android Logcat
        if (nonce % 5 == 0) {
            std::string hash_str = bytesToHexString(result_hash, 32);
            LOGI("小菊持續運算中... 測試資料雙重雜湊結果: %s", hash_str.c_str());
        }
    }
}

// 提供給前端 Kotlin 讀取引擎狀態的介面
extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string status = "小月與小菊引擎就緒。SHA-256 模組已掛載。";
    return env->NewStringUTF(status.c_str());
}

// 提供給前端 Kotlin 觸發啟動背景挖礦的介面
extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    // 建立分離的執行緒 (Detached Thread) 執行 startMiningLoop
    // 必須使用獨立執行緒，否則無窮迴圈會卡死 Android UI 導致程式崩潰 (ANR)
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}