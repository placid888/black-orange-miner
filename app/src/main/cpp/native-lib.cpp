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
#include <atomic>
#include <chrono>
#include "sha256.h"

#define LOG_TAG "ManekiMiner-Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char* POOL_HOST = "solo.ckpool.org";
const int POOL_PORT = 3333;
const char* WALLET_ADDRESS = "bc1qvn2rhjw553l2ttplyqpt2kaepd32dh5q6kfac9";

JavaVM* g_jvm = nullptr;
jobject g_main_activity = nullptr;

// 新增：油門狀態全域變數，預設為全速
std::atomic<bool> g_is_full_speed(true);

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

void updateUI(const char* status, uint32_t nonce, const char* hash) {
    if (g_jvm == nullptr || g_main_activity == nullptr) return;

    JNIEnv* env;
    if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;

    jclass clazz = env->GetObjectClass(g_main_activity);
    jmethodID methodID = env->GetMethodID(clazz, "updateMiningStatus", "(Ljava/lang/String;ILjava/lang/String;)V");

    if (methodID != nullptr) {
        jstring jStatus = env->NewStringUTF(status);
        jstring jHash = env->NewStringUTF(hash);
        env->CallVoidMethod(g_main_activity, methodID, jStatus, nonce, jHash);
        env->DeleteLocalRef(jStatus);
        env->DeleteLocalRef(jHash);
    }
    
    g_jvm->DetachCurrentThread();
}

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
    updateUI("引擎啟動中，準備連線...", 0, "");
    
    while (true) {
        LOGI("連線至 Solo 礦池 %s:%d", POOL_HOST, POOL_PORT);

        struct hostent *host = gethostbyname(POOL_HOST);
        if (host == nullptr) {
            updateUI("DNS 解析失敗，等待重試", 0, "");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            updateUI("Socket 建立失敗", 0, "");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(POOL_PORT);
        memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            updateUI("礦池伺服器連線失敗", 0, "");
            close(sock);
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        
        updateUI("成功連線至礦池", 0, "");

        const char* subscribe_msg = "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"ManekiMiner/1.0\"]}\n";
        send(sock, subscribe_msg, strlen(subscribe_msg), 0);
        
        char authorize_msg[256];
        snprintf(authorize_msg, sizeof(authorize_msg), "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"x\"]}\n", WALLET_ADDRESS);
        send(sock, authorize_msg, strlen(authorize_msg), 0);

        std::atomic<bool> is_connected(true);
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

        std::thread listener_thread([sock, &is_connected, &current_extranonce1, &current_extranonce2_size, &current_job_id, &current_prevhash, &current_coinb1, &current_coinb2, &current_version, &current_nbit, &current_ntime, &current_merkle_branch, &target_difficulty]() {
            char rx_buffer[4096];
            while (is_connected) {
                memset(rx_buffer, 0, sizeof(rx_buffer));
                int bytes_received = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                if (bytes_received <= 0) {
                    is_connected = false;
                    break;
                }

                std::string payload(rx_buffer);
                
                if (payload.find("\"id\": 1") != std::string::npos || payload.find("\"id\":1") != std::string::npos) {
                    std::regex sub_regex(R"REGEX("result":\s*\[.*,\s*"([a-fA-F0-9]+)",\s*(\d+)\s*\])REGEX");
                    std::smatch match;
                    if (std::regex_search(payload, match, sub_regex) && match.size() >= 3) {
                        current_extranonce1 = match.str(1);
                        current_extranonce2_size = std::stoi(match.str(2));
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
                    }
                }
            }
        });
        listener_thread.detach();

        uint32_t nonce = 0;
        uint32_t extranonce2_val = 0;
        uint8_t hash_output[32];

        while (is_connected) {
            if (current_prevhash.empty() || current_extranonce1.empty() || current_version.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            // 新增：油門控制邏輯 (Throttling)
            if (!g_is_full_speed) {
                // 低功耗模式：每 1000 次運算強制休眠 10 毫秒，釋放逾 90% CPU 資源並防止過熱
                if (nonce % 1000 == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
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
                updateUI("找到區塊並提交中！", nonce, success_hash.c_str());
                
                char nonce_hex[9];
                snprintf(nonce_hex, sizeof(nonce_hex), "%08x", nonce);

                char submit_msg[512];
                snprintf(submit_msg, sizeof(submit_msg), 
                         "{\"id\": 4, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]}\n", 
                         WALLET_ADDRESS, current_job_id.c_str(), extranonce2.c_str(), current_ntime.c_str(), nonce_hex);
                
                send(sock, submit_msg, strlen(submit_msg), 0);
            }

            nonce++;
            if (nonce == 0xFFFFFFFF) {
                extranonce2_val++;
                nonce = 0;
            }

            if (nonce % 100000 == 0) {
                std::string block_hash_hex = bytesToHexString(hash_output, 32);
                // 根據全域變數動態更新 UI 文字
                if (g_is_full_speed) {
                    updateUI("全速運算中", nonce, block_hash_hex.c_str());
                } else {
                    updateUI("低功耗運算中", nonce, block_hash_hex.c_str());
                }
            }
        }

        updateUI("連線中斷，準備重新連線", 0, "");
        close(sock);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string status = "引擎就緒。UI 回呼機制已掛載。";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    if (g_main_activity != nullptr) {
        env->DeleteGlobalRef(g_main_activity);
    }
    g_main_activity = env->NewGlobalRef(thiz);
    
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}

// 新增：JNI 油門控制接收器
extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_setMiningIntensity(JNIEnv *env, jobject thiz, jboolean is_full_speed) {
    g_is_full_speed = is_full_speed;
    if (is_full_speed) {
        LOGI("引擎控制：切換至全速運算模式");
    } else {
        LOGI("引擎控制：切換至低功耗運算模式");
    }
}