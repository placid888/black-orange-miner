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
#include <cstdlib>
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
        out[i/2] = (uint8_t) strtoul(byteString.c_str(), nullptr, 16);
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

std::vector<std::string> extractArrayElements(const std::string& arrayStr) {
    std::vector<std::string> elements;
    std::regex str_regex(R"REGEX("([^"]+)")REGEX");
    auto words_begin = std::sregex_iterator(arrayStr.begin(), arrayStr.end(), str_regex);
    auto words_end = std::sregex_iterator();
    for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
        elements.push_back(i->str(1));
    }
    return elements;
}

void getTargetFromNbits(const std::string& nbits_hex, uint8_t* target) {
    memset(target, 0, 32);
    uint32_t bits = strtoul(nbits_hex.c_str(), nullptr, 16);
    uint8_t shift = (bits >> 24) & 0xFF;
    uint32_t mantissa = bits & 0x00FFFFFF;

    if (shift <= 32 && shift >= 3) {
        target[32 - shift] = (mantissa >> 16) & 0xFF;
        target[32 - shift + 1] = (mantissa >> 8) & 0xFF;
        target[32 - shift + 2] = mantissa & 0xFF;
    }
}

bool checkHashMeetsTarget(const uint8_t* hash, const uint8_t* target) {
    for (int i = 0; i < 32; ++i) {
        uint8_t hash_byte = hash[31 - i];
        if (hash_byte > target[i]) return false;
        if (hash_byte < target[i]) return true;
    }
    return true;
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
    std::string current_version = "";
    std::string current_nbit = "";
    std::string current_ntime = "";
    std::vector<std::string> current_merkle_branch;
    
    uint8_t target_difficulty[32];
    memset(target_difficulty, 0, 32);

    std::thread listener_thread([sock, &current_extranonce1, &current_extranonce2_size, &current_job_id, &current_prevhash, &current_coinb1, &current_coinb2, &current_version, &current_nbit, &current_ntime, &current_merkle_branch, &target_difficulty]() {
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
                std::regex notify_regex(R"REGEX("params":\s*\[\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*\[(.*?)\]\s*,\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)")REGEX");
                std::smatch match;
                if (std::regex_search(payload, match, notify_regex) && match.size() >= 9) {
                    current_job_id = match.str(1);
                    current_prevhash = match.str(2);
                    current_coinb1 = match.str(3);
                    current_coinb2 = match.str(4);
                    std::string merkle_array_str = match.str(5);
                    current_version = match.str(6);
                    current_nbit = match.str(7);
                    current_ntime = match.str(8);
                    
                    current_merkle_branch = extractArrayElements(merkle_array_str);
                    getTargetFromNbits(current_nbit, target_difficulty);
                    
                    LOGI("【取得新任務】 Job ID: %s, 目標難度已更新", current_job_id.c_str());
                }
            }
        }
    });
    listener_thread.detach();

    uint32_t nonce = 0;
    uint32_t extranonce2_val = 0;
    uint8_t hash_output[32];

    while (true) {
        if (current_prevhash.empty() || current_extranonce1.empty() || current_version.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        std::stringstream en2_ss;
        en2_ss << std::hex << std::setw(current_extranonce2_size * 2) << std::setfill('0') << extranonce2_val;
        std::string extranonce2 = en2_ss.str();

        std::string coinbase_hex = current_coinb1 + current_extranonce1 + extranonce2 + current_coinb2;
        
        size_t coinbase_len = coinbase_hex.length() / 2;
        uint8_t coinbase_bytes[1024]; 
        hexStringToBytes(coinbase_hex, coinbase_bytes);

        uint8_t merkle_root[32];
        double_sha256(coinbase_bytes, coinbase_len, merkle_root);

        for (const std::string& branch_hex : current_merkle_branch) {
            uint8_t branch_bytes[32];
            hexStringToBytes(branch_hex, branch_bytes);
            
            uint8_t concat[64];
            memcpy(concat, merkle_root, 32);
            memcpy(concat + 32, branch_bytes, 32);
            
            double_sha256(concat, 64, merkle_root);
        }

        uint8_t block_header[80];
        memset(block_header, 0, sizeof(block_header));

        hexStringToBytes(current_version, block_header);
        reverseBytes(block_header, 4);

        hexStringToBytes(current_prevhash, block_header + 4);

        memcpy(block_header + 36, merkle_root, 32);

        hexStringToBytes(current_ntime, block_header + 68);
        reverseBytes(block_header + 68, 4);

        hexStringToBytes(current_nbit, block_header + 72);
        reverseBytes(block_header + 72, 4);

        block_header[76] = (nonce >> 0) & 0xFF;
        block_header[77] = (nonce >> 8) & 0xFF;
        block_header[78] = (nonce >> 16) & 0xFF;
        block_header[79] = (nonce >> 24) & 0xFF;

        double_sha256(block_header, 80, hash_output);

        if (checkHashMeetsTarget(hash_output, target_difficulty)) {
            std::string success_hash = bytesToHexString(hash_output, 32);
            LOGI("★★★★★【碰撞成功】★★★★★ 找到符合難度的區塊！");
            LOGI("Hash: %s", success_hash.c_str());
            LOGI("Nonce: %u, Extranonce2: %s", nonce, extranonce2.c_str());
            
            char nonce_hex[9];
            snprintf(nonce_hex, sizeof(nonce_hex), "%08x", nonce);

            char submit_msg[512];
            snprintf(submit_msg, sizeof(submit_msg), 
                     "{\"id\": 4, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]}\n", 
                     WALLET_ADDRESS, current_job_id.c_str(), extranonce2.c_str(), current_ntime.c_str(), nonce_hex);
            
            send(sock, submit_msg, strlen(submit_msg), 0);
            LOGI("已發送 mining.submit 指令：\n%s", submit_msg);
        }

        nonce++;
        if (nonce == 0xFFFFFFFF) {
            extranonce2_val++;
            nonce = 0;
        }

        if (nonce % 100000 == 0) {
            std::string block_hash_hex = bytesToHexString(hash_output, 32);
            LOGI("小菊運算中... Nonce: %u, 區塊雜湊: %s", nonce, block_hash_hex.c_str());
        }
    }

    close(sock);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string status = "引擎就緒。任務提交模組已掛載。";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}