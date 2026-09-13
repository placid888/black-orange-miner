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
#include <vector>
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

void hexStringToBytes(const std::string& hex, uint8_t* out) {
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        out[i/2] = (uint8_t) strtol(byteString.c_str(), nullptr, 16);
    }
}

void reverseBytes(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len / 2; ++i) {
        std::swap(data[i], data[len - 1 - i]);
    }
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

// 解析 JSON 陣列字串提取元素
std::vector<std::string> extractArrayElements(const std::string& arrayStr) {
    std::vector<std::string> elements;
    std::regex str_regex(R"("([^"]+)")");
    auto words_begin = std::sregex_iterator(arrayStr.begin(), arrayStr.end(), str_regex);
    auto words_end = std::sregex_iterator();
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        elements.push_back(i->str(1));
    }
    return elements;
}

void startMiningLoop() {
    LOGI("小菊全速模式啟動：嘗試連線至 Solo 礦池 %s:%d", POOL_HOST, POOL_PORT);

    struct hostent *host = gethostbyname(POOL_HOST);
    if (host == nullptr) {
        LOGE("DNS 解析失敗，檢查網路連線狀態");
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

    std::string current_extranonce1 = "";
    int current_extranonce2_size = 0;
    
    std::string current_job_id = "";
    std::string current_prevhash = "";
    std::string current_coinb1 = "";
    std::string current_coinb2 = "";
    std::string current_nbit = "";
    std::string current_ntime = "";
    std::vector<std::string> current_merkle_branch;

    std::thread listener_thread([sock, &current_extranonce1, &current_extranonce2_size, &current_job_id, &current_prevhash, &current_coinb1, &current_coinb2, &current_nbit, &current_ntime, &current_merkle_branch]() {
        char rx_buffer[4096];
        while (true) {
            memset(rx_buffer, 0, sizeof(rx_buffer));
            int bytes_received = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            if (bytes_received <= 0) break;

            std::string payload(rx_buffer);
            
            if (payload.find("\"id\": 1") != std::string::npos || payload.find("\"id\":1") != std::string::npos) {
                std::regex sub_regex(R"REGEX("result":\s*\[.*,\s*"([a-fA-F0-9]+)",\s*(\d+)\s*\])REGEX");
                std::smatch match;
                if (std::regex_search(payload, match, sub_regex) && match.size() >= 3) {
                    current_extranonce1 = match.str(1);
                    current_extranonce2_size = std::stoi(match.str(2));
                    LOGI("【訂閱解析成功】 Extranonce1: %s, Size: %d", current_extranonce1.c_str(), current_extranonce2_size);
                }
            }

            if (payload.find("mining.notify") != std::string::npos) {
                // 調整正規表達式，提取包含 merkle_branch 陣列的原始字串
                std::regex notify_regex(R"REGEX("params":\s*\[\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*\[(.*?)\]\s*,\s*"([^"]+)",\s*"([^"]+)")REGEX");
                std::smatch match;
                if (std::regex_search(payload, match, notify_regex) && match.size() >= 8) {
                    current_job_id = match.str(1);
                    current_prevhash = match.str(2);
                    current_coinb1 = match.str(3);
                    current_coinb2 = match.str(4);
                    std::string merkle_array_str = match.str(5);
                    current_nbit = match.str(6);
                    current_ntime = match.str(7);
                    
                    current_merkle_branch = extractArrayElements(merkle_array_str);
                    LOGI("【取得新任務】 Job ID: %s, 梅克爾分支數量: %zu", current_job_id.c_str(), current_merkle_branch.size());
                }
            }
        }
    });
    listener_thread.detach();

    uint32_t nonce = 0;
    uint32_t extranonce2_val = 0;
    uint8_t hash_output[32];

    while (true) {
        if (current_prevhash.empty() || current_extranonce1.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // 格式化 extranonce2 (補零至指定長度，通常為 8 個 16 進位字元 = 4 bytes)
        std::stringstream en2_ss;
        en2_ss << std::hex << std::setw(current_extranonce2_size * 2) << std::setfill('0') << extranonce2_val;
        std::string extranonce2 = en2_ss.str();

        // 組合 Coinbase 交易字串
        std::string coinbase_hex = current_coinb1 + current_extranonce1 + extranonce2 + current_coinb2;

        char header_buf[512];
        snprintf(header_buf, sizeof(header_buf), "%s-%u-%s", current_prevhash.c_str(), nonce, current_ntime.c_str());
        double_sha256(reinterpret_cast<const uint8_t*>(header_buf), strlen(header_buf), hash_output);

        nonce++;
        if (nonce == 0xFFFFFFFF) {
            extranonce2_val++;
            nonce = 0;
        }

        if (nonce % 100000 == 0) {
            LOGI("小菊運算中... Nonce: %u, Coinbase: %s...", nonce, coinbase_hex.substr(0, 32).c_str());
        }
    }

    close(sock);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string status = "引擎就緒。梅克爾與 Coinbase 模組掛載。";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}