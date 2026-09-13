#include <jni.h>
#include <string>
#include <thread>
#include <android/log.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define LOG_TAG "ManekiMiner-Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void startMiningLoop() {
    LOGI("小菊全速模式啟動：準備建立 TCP Socket 與 Stratum 連線");
    
    uint32_t nonce = 0;
    while(true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
        nonce++;
        if (nonce % 10 == 0) {
            LOGI("小菊已完成 %d 次虛擬雜湊碰撞測試", nonce);
        }
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string status = "小月與小菊引擎就緒。等待 Socket 連線指令。";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}