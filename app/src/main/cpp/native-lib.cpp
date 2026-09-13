#include <jni.h>
#include <string>
#include <thread>
#include <android/log.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cstdio>
#include "sha256.h"

#define LOG_TAG "ManekiMiner-Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char* POOL_HOST = "solo.ckpool.org";
const int POOL_PORT = 3333;

// 請保留已設定好的比特幣地址
const char* WALLET_ADDRESS = "bc1qvn2rhjw553l2ttplyqpt2kaepd32dh5q6kfac9";

std::string bytesToHexString(const uint8_t* bytes, size_t len) {
    std::stringstream ss;
    for(size_t i = 0; i < len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)bytes[i];
    }
    return ss.str();
}

void double_sha256(const uint8_t* data, size_t len, uint8_t* hash_out) {
    SHA256_CTX ctx;
    uint8_t first_hash[32];

    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, first_hash);

    sha256_init(&ctx);
    sha256_update(&ctx, first_hash, 32);
    sha256_final(&ctx, hash_out);
}

void startMiningLoop() {
    LOGI("小菊全速模式啟動：嘗試連線至 Solo 礦池 %s:%d", POOL_HOST, POOL_PORT);

    struct hostent *host = gethostbyname(POOL_HOST);
    if (host == nullptr) {
        LOGE("DNS 解析失敗，請檢查網路連線狀態");
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGE("Socket 建立失敗");
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(POOL_PORT);
    memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOGE("連線至礦池伺服器失敗");
        close(sock);
        return;
    }
    LOGI("成功連線至礦池伺服器！");

    const char* subscribe_msg = "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"ManekiMiner/1.0\"]}\n";
    send(sock, subscribe_msg, strlen(subscribe_msg), 0);
    
    char authorize_msg[256];
    snprintf(authorize_msg, sizeof(authorize_msg), "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"x\"]}\n", WALLET_ADDRESS);
    send(sock, authorize_msg, strlen(authorize_msg), 0);

    LOGI("小菊完成授權，進入持續監聽任務模式...");

    // 建立無窮迴圈，持續接收礦池派發的新區塊任務
    char buffer[4096];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0) {
            // 透過字串比對過濾不同類型的礦池指令
            if (strstr(buffer, "mining.notify") != nullptr) {
                LOGI("收到新區塊挖礦任務 (mining.notify)：\n%s", buffer);
            } else if (strstr(buffer, "mining.set_difficulty") != nullptr) {
                LOGI("收到難度調整指令 (mining.set_difficulty)：\n%s", buffer);
            } else {
                LOGI("收到礦池訊息：\n%s", buffer);
            }
        } else if (bytes_received == 0) {
            LOGE("礦池伺服器已關閉連線");
            break;
        } else {
            LOGE("網路連線異常中斷");
            break;
        }
    }

    close(sock);
    LOGI("連線結束，退出小菊全速模式");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string status = "小月與小菊引擎就緒。Stratum 任務監聽中。";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}