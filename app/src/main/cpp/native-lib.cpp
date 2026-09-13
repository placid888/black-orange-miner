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
#include <regex>
#include "sha256.h"

#define LOG_TAG "ManekiMiner-Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char* POOL_HOST = "solo.ckpool.org";
const int POOL_PORT = 3333;

// 務必保留已設定好的比特幣地址
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

    char buffer[4096];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

        if (bytes_received > 0) {
            std::string payload(buffer);
            
            // 判斷是否為新區塊任務封包
            if (payload.find("mining.notify") != std::string::npos) {
                // 使用正則表達式提取 params 陣列中的前四個字串參數
                // 格式對應: ["job_id", "prevhash", "coinb1", "coinb2", ...
                std::regex notify_regex(R"("params":\s*\[\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)")");
                std::smatch match;
                
                if (std::regex_search(payload, match, notify_regex) && match.size() >= 5) {
                    std::string job_id = match.str(1);
                    std::string prevhash = match.str(2);
                    std::string coinb1 = match.str(3);
                    std::string coinb2 = match.str(4);
                    
                    LOGI("【任務解析成功】 Job ID: %s", job_id.c_str());
                    LOGI("前一區塊雜湊 (prevhash): %s", prevhash.c_str());
                } else {
                    LOGE("任務解析失敗，封包格式不符或參數缺失");
                }
            } 
            // 判斷是否為難度調整封包
            else if (payload.find("mining.set_difficulty") != std::string::npos) {
                std::regex diff_regex(R"("params":\s*\[\s*([0-9.]+)\s*\])");
                std::smatch match;
                
                if (std::regex_search(payload, match, diff_regex) && match.size() >= 2) {
                    std::string difficulty = match.str(1);
                    LOGI("【難度調整】 當前目標難度: %s", difficulty.c_str());
                }
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
    std::string status = "小月與小菊引擎就緒。任務解析模組已掛載。";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}