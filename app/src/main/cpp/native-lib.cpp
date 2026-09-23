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
#include <mutex>
#include <sched.h>
#include <pthread.h>
#include "sha256.h"

#define LOG_TAG "ManekiMiner-Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

const char* POOL_HOST = "solo.ckpool.org";
const int POOL_PORT = 3333;
const char* WALLET_ADDRESS = "bc1qldm4e3yzaq5acf5sgg6m0486a2hvgkx370w6sx";

JavaVM* g_jvm = nullptr;
jobject g_main_activity = nullptr;

std::atomic<bool> g_is_full_speed(true);
std::atomic<bool> g_is_mining_running(false);
std::atomic<int> g_current_sock(-1);
std::atomic<int> g_accepted_shares(0);

std::atomic<bool> g_has_valid_job(false);
std::atomic<uint32_t> g_job_version(0);
uint8_t g_shared_header[80];
uint8_t g_shared_target[32];
std::string g_shared_job_id = "-";
std::string g_shared_extranonce2 = "";
std::string g_shared_ntime = "";
std::string g_active_nbit_ui = "-";

std::atomic<uint64_t> g_job_start_timestamp(0);
std::atomic<long> g_prev_round_time_sec(0);
std::atomic<uint64_t> g_total_hashes(0);

std::mutex g_submit_mutex;
std::mutex g_ui_mutex;
std::string g_sample_hash = "";
uint32_t g_sample_nonce = 0;

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

void updateUI(const char* status, uint32_t nonce, const char* hash, double hashrate, int shares, long uptime, const char* job_id, const char* difficulty, long round_time, long prev_round_time) {
    if (g_jvm == nullptr || g_main_activity == nullptr) return;
    JNIEnv* env;
    if (g_jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    jclass clazz = env->GetObjectClass(g_main_activity);
    jmethodID methodID = env->GetMethodID(clazz, "updateMiningStatus", "(Ljava/lang/String;ILjava/lang/String;DIJLjava/lang/String;Ljava/lang/String;JJ)V");
    if (methodID != nullptr) {
        jstring jStatus = env->NewStringUTF(status);
        jstring jHash = env->NewStringUTF(hash);
        jstring jJobId = env->NewStringUTF(job_id);
        jstring jDifficulty = env->NewStringUTF(difficulty);
        env->CallVoidMethod(g_main_activity, methodID, jStatus, nonce, jHash, hashrate, shares, (jlong)uptime, jJobId, jDifficulty, (jlong)round_time, (jlong)prev_round_time);
        env->DeleteLocalRef(jStatus);
        env->DeleteLocalRef(jHash);
        env->DeleteLocalRef(jJobId);
        env->DeleteLocalRef(jDifficulty);
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

void minerWorker(int thread_id, int num_threads) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    if (num_threads >= 8) {
        int target_core = 4 + (thread_id % 4);
        CPU_SET(target_core, &cpuset);
    } else {
        CPU_SET(thread_id % num_threads, &cpuset);
    }
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);

    uint8_t local_header[80];
    uint8_t local_target[32];
    uint8_t hash_output[32];
    uint32_t nonce = 0;

    while (g_is_mining_running) {
        if (!g_has_valid_job) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        uint32_t current_version = g_job_version.load();
        
        {
            std::lock_guard<std::mutex> lock(g_submit_mutex);
            memcpy(local_header, g_shared_header, 80);
            memcpy(local_target, g_shared_target, 32);
        }

        SHA256_CTX midstate;
        sha256_init(&midstate);
        sha256_update(&midstate, local_header, 64);

        nonce = thread_id;
        uint64_t local_hashes = 0;

        while (g_is_mining_running && g_job_version.load() == current_version) {
            if (!g_is_full_speed) {
                if (local_hashes % 500 == 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            local_header[76] = (nonce >> 0) & 0xFF;
            local_header[77] = (nonce >> 8) & 0xFF;
            local_header[78] = (nonce >> 16) & 0xFF;
            local_header[79] = (nonce >> 24) & 0xFF;

            SHA256_CTX ctx1 = midstate;
            sha256_update(&ctx1, local_header + 64, 16);
            uint8_t first_hash[32];
            sha256_final(&ctx1, first_hash);

            SHA256_CTX ctx2;
            sha256_init(&ctx2);
            sha256_update(&ctx2, first_hash, 32);
            sha256_final(&ctx2, hash_output);

            local_hashes++;

            if (checkHashMeetsTarget(hash_output, local_target)) {
                std::lock_guard<std::mutex> lock(g_submit_mutex);
                g_accepted_shares++;
                std::string success_hash = bytesToHexString(hash_output, 32);
                
                updateUI("🎯 找到區塊並提交中！", nonce, success_hash.c_str(), 0.0, g_accepted_shares.load(), 0, g_shared_job_id.c_str(), g_active_nbit_ui.c_str(), 0, 0); 

                char nonce_hex[9];
                snprintf(nonce_hex, sizeof(nonce_hex), "%08x", nonce);
                char submit_msg[512];
                snprintf(submit_msg, sizeof(submit_msg), 
                         "{\"id\": 4, \"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"]}\n", 
                         WALLET_ADDRESS, g_shared_job_id.c_str(), g_shared_extranonce2.c_str(), g_shared_ntime.c_str(), nonce_hex);
                
                int sock = g_current_sock.load();
                if (sock >= 0) send(sock, submit_msg, strlen(submit_msg), 0);
            }

            if (thread_id == 0 && (local_hashes == 4999)) {
                std::lock_guard<std::mutex> lock(g_ui_mutex);
                g_sample_hash = bytesToHexString(hash_output, 32);
                g_sample_nonce = nonce;
            }

            if (local_hashes >= 5000) {
                g_total_hashes += local_hashes;
                local_hashes = 0;
            }

            nonce += num_threads;
        }
        
        if (local_hashes > 0) g_total_hashes += local_hashes;
    }
}

void networkLoop() {
    auto session_start_time = std::chrono::steady_clock::now();
    auto last_ui_update = std::chrono::steady_clock::now();

    updateUI("🟡 引擎啟動中，準備連線...", 0, "", 0.0, g_accepted_shares.load(), 0, "-", "-", 0, 0);
    
    while (g_is_mining_running) {
        g_has_valid_job = false;
        long uptime_sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - session_start_time).count();
        
        struct hostent *host = gethostbyname(POOL_HOST);
        if (host == nullptr) {
            updateUI("🔴 DNS 解析失敗", 0, "", 0.0, g_accepted_shares.load(), uptime_sec, "-", "-", 0, 0);
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }
        
        g_current_sock = sock;
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(POOL_PORT);
        memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            updateUI("🔴 礦池連線失敗", 0, "", 0.0, g_accepted_shares.load(), uptime_sec, "-", "-", 0, 0);
            close(sock);
            g_current_sock = -1;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        const char* subscribe_msg = "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"ManekiMiner/1.0\"]}\n";
        send(sock, subscribe_msg, strlen(subscribe_msg), 0);
        
        char authorize_msg[256];
        snprintf(authorize_msg, sizeof(authorize_msg), "{\"id\": 2, \"method\": \"mining.authorize\", \"params\": [\"%s\", \"x\"]}\n", WALLET_ADDRESS);
        send(sock, authorize_msg, strlen(authorize_msg), 0);

        std::atomic<bool> is_connected(true);
        std::string current_extranonce1 = "";
        int current_extranonce2_size = 0;
        std::string local_current_job_id = "-";

        std::thread listener_thread([sock, &is_connected, &current_extranonce1, &current_extranonce2_size, &local_current_job_id]() {
            char rx_buffer[4096];
            while (is_connected && g_is_mining_running) {
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
                
                if (payload.find("mining.notify") != std::string::npos && !current_extranonce1.empty()) {
                    std::regex notify_regex(R"REGEX("params":\s*\[\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*\[(.*?)\]\s*,\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)")REGEX");
                    std::smatch match;
                    if (std::regex_search(payload, match, notify_regex) && match.size() >= 9) {
                        std::string new_job_id = match.str(1);
                        
                        if (local_current_job_id != "-" && local_current_job_id != new_job_id) {
                            uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                            uint64_t start_ms = g_job_start_timestamp.load();
                            if (start_ms > 0) g_prev_round_time_sec = (now_ms - start_ms) / 1000;
                            g_job_start_timestamp = now_ms; 
                        } else if (local_current_job_id == "-") {
                            g_job_start_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        }
                        local_current_job_id = new_job_id;

                        std::stringstream en2_ss;
                        en2_ss << std::hex << std::setw(current_extranonce2_size * 2) << std::setfill('0') << 0;
                        std::string extranonce2 = en2_ss.str();
                        
                        std::string coinbase_hex = match.str(3) + current_extranonce1 + extranonce2 + match.str(4);
                        size_t coinbase_len = coinbase_hex.length() / 2;
                        uint8_t coinbase_bytes[1024]; 
                        hexStringToBytes(coinbase_hex, coinbase_bytes);

                        uint8_t merkle_root[32];
                        double_sha256(coinbase_bytes, coinbase_len, merkle_root);
                        std::vector<std::string> branches = extractArrayElements(match.str(5));
                        for (const std::string& branch_hex : branches) {
                            uint8_t branch_bytes[32];
                            hexStringToBytes(branch_hex, branch_bytes);
                            uint8_t concat[64];
                            memcpy(concat, merkle_root, 32);
                            memcpy(concat + 32, branch_bytes, 32);
                            double_sha256(concat, 64, merkle_root);
                        }

                        uint8_t new_header[80];
                        memset(new_header, 0, sizeof(new_header));
                        hexStringToBytes(match.str(6), new_header); 
                        reverseBytes(new_header, 4);
                        hexStringToBytes(match.str(2), new_header + 4); 
                        memcpy(new_header + 36, merkle_root, 32);
                        hexStringToBytes(match.str(8), new_header + 68); 
                        reverseBytes(new_header + 68, 4);
                        hexStringToBytes(match.str(7), new_header + 72); 
                        reverseBytes(new_header + 72, 4);
                        
                        uint8_t new_target[32];
                        getTargetFromNbits(match.str(7), new_target);

                        {
                            std::lock_guard<std::mutex> lock(g_submit_mutex);
                            memcpy(g_shared_header, new_header, 80);
                            memcpy(g_shared_target, new_target, 32);
                            g_shared_job_id = new_job_id;
                            g_shared_extranonce2 = extranonce2;
                            g_shared_ntime = match.str(8);
                            g_active_nbit_ui = match.str(7);
                            
                            g_job_version++;
                            g_has_valid_job = true;
                        }
                    }
                }
            }
        });
        listener_thread.detach();

        while (is_connected && g_is_mining_running) {
            // ==========================================
            // 🚀 解除 FPS 封印：從每秒 1 次提升至每秒 5 次 (200毫秒)
            // ==========================================
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            auto now = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(now - last_ui_update).count();
            uint64_t hashes = g_total_hashes.exchange(0);
            double hashrate = hashes / elapsed;
            last_ui_update = now;
            
            uptime_sec = std::chrono::duration_cast<std::chrono::seconds>(now - session_start_time).count();
            uint64_t now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
            uint64_t start_ms = g_job_start_timestamp.load();
            long current_round_sec = (start_ms > 0) ? (now_ms - start_ms) / 1000 : 0;
            long prev_sec = g_prev_round_time_sec.load();
            
            std::string sample_hash;
            uint32_t sample_nonce = 0;
            {
                std::lock_guard<std::mutex> lock(g_ui_mutex);
                sample_hash = g_sample_hash;
                sample_nonce = g_sample_nonce;
            }
            
            std::string job_id_ui;
            std::string nbit_ui;
            {
                std::lock_guard<std::mutex> lock(g_submit_mutex);
                job_id_ui = g_shared_job_id;
                nbit_ui = g_active_nbit_ui;
            }

            if (g_has_valid_job) {
                if (g_is_full_speed) {
                    updateUI("🟢 運算中 (多核並行🚀)", sample_nonce, sample_hash.c_str(), hashrate, g_accepted_shares.load(), uptime_sec, job_id_ui.c_str(), nbit_ui.c_str(), current_round_sec, prev_sec);
                } else {
                    updateUI("🟡 節能運算中 (多核限制)", sample_nonce, sample_hash.c_str(), hashrate, g_accepted_shares.load(), uptime_sec, job_id_ui.c_str(), nbit_ui.c_str(), current_round_sec, prev_sec);
                }
            } else {
                updateUI("🟢 成功連線，等待任務...", 0, "", 0.0, g_accepted_shares.load(), uptime_sec, "-", "-", 0, 0);
            }
        }

        int current_sock = g_current_sock.exchange(-1);
        if (current_sock >= 0) close(current_sock);

        uptime_sec = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - session_start_time).count();
        if (g_is_mining_running) {
            updateUI("🔴 連線中斷，準備重新連線", 0, "", 0.0, g_accepted_shares.load(), uptime_sec, "-", "-", 0, 0);
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(JNIEnv* env, jobject /* this */) {
    return env->NewStringUTF("引擎就緒。儀表板連線中...");
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    if (g_main_activity != nullptr) env->DeleteGlobalRef(g_main_activity);
    g_main_activity = env->NewGlobalRef(thiz);
    
    if (!g_is_mining_running.exchange(true)) {
        std::thread netThread(networkLoop);
        netThread.detach();

        unsigned int num_cores = std::thread::hardware_concurrency();
        if (num_cores == 0) num_cores = 4;
        LOGI("啟動多核引擎，共分配 %d 個 CPU 核心！", num_cores);

        for (unsigned int i = 0; i < num_cores; ++i) {
            std::thread worker(minerWorker, i, num_cores);
            worker.detach();
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_setMiningIntensity(JNIEnv *env, jobject thiz, jboolean is_full_speed) {
    g_is_full_speed = is_full_speed;
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_stopMiningNative(JNIEnv *env, jobject thiz) {
    g_is_mining_running = false;
    int sock = g_current_sock.exchange(-1);
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
}