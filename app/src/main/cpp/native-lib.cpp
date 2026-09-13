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

const char* WALLET_ADDRESS = "請貼上比特幣地址";

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

    LOGI("小菊完成授權，進入挖礦與任務監聽迴圈...");

    char buffer[4096];
    std::string current_job_id = "";
    std::string current_prevhash = "";
    std::string current_coinb1 = "";
    std::string current_coinb2 = "";
    std::string current_nbit = "";
    std::string current_ntime = "";

    // 設定 Socket 為非阻塞或透過執行緒同步讀取
    // 此處簡化架構：啟動獨立運算迴圈，背景同時監聽伺服器封包
    std::thread listener_thread([sock, &current_job_id, &current_prevhash, &current_coinb1, &current_coinb2, &current_nbit, &current_ntime]() {
        char rx_buffer[4096];
        while (true) {
            memset(rx_buffer, 0, sizeof(rx_buffer));
            int bytes_received = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (bytes_received <= 0) break;

            std::string payload(rx_buffer);
            if (payload.find("mining.notify") != std::string::npos) {
                std::regex notify_regex(R"REGEX("params":\s*\[\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*\[[^\]]*\],\s*"([^"]+)",\s*"([^"]+)")REGEX");
                std::smatch match;
                if (std::regex_search(payload, match, notify_regex) && match.size() >= 7) {
                    current_job_id = match.str(1);
                    current_prevhash = match.str(2);
                    current_coinb1 = match.str(3);
                    current_coinb2 = match.str(4);
                    current_nbit = match.str(5);
                    current_ntime = match.str(6);
                    LOGI("【取得新任務】 Job ID: %s", current_job_id.c_str());
                }
            }
        }
    });
    listener_thread.detach();

    // 挖礦碰撞主迴圈
    uint32_t nonce = 0;
    uint8_t hash_output[32];

    while (true) {
        if (current_prevhash.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // 組合簡易區塊標頭進行雙重雜湊測試
        char header_buf[512];
        snprintf(header_buf, sizeof(header_buf), "%s-%u-%s", current_prevhash.c_str(), nonce, current_ntime.c_str());
        
        double_sha256(reinterpret_cast<const uint8_t*>(header_buf), strlen(header_buf), hash_output);

        nonce++;

        if (nonce % 100000 == 0) {
            LOGI("小菊運算中... 當前 Nonce: %u", nonce);
        }
    }

    close(sock);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string status = "小月與小菊引擎就緒。挖礦核心運算中。";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}