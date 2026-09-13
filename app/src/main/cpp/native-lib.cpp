#include <jni.h>
#include <string>
#include <thread>
#include <android/log.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>      // 用於 DNS 網域解析 (gethostbyname)
#include <iomanip>
#include <sstream>
#include <cstring>      // 用於 memset 操作記憶體
#include "sha256.h"

#define LOG_TAG "ManekiMiner-Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 礦池設定 (此為 CKPool 的 Solo 挖礦伺服器)
const char* POOL_HOST = "solo.ckpool.org";
const int POOL_PORT = 3333;

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

// 網路連線與通訊主迴圈
void startMiningLoop() {
    LOGI("小菊全速模式啟動：嘗試連線至 Solo 礦池 %s:%d", POOL_HOST, POOL_PORT);

    // 步驟 1：解析礦池網域名稱 (DNS Lookup)
    struct hostent *host = gethostbyname(POOL_HOST);
    if (host == nullptr) {
        LOGE("DNS 解析失敗，請檢查網路連線狀態");
        return;
    }

    // 步驟 2：建立 TCP Socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        LOGE("Socket 建立失敗");
        return;
    }

    // 步驟 3：設定伺服器位址結構
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(POOL_PORT);
    memcpy(&server_addr.sin_addr.s_addr, host->h_addr, host->h_length);

    // 步驟 4：發起連線
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        LOGE("連線至礦池伺服器失敗");
        close(sock);
        return;
    }
    LOGI("成功連線至礦池伺服器！");

    // 步驟 5：發送 Stratum 訂閱指令 (mining.subscribe)
    // 這是與礦池溝通的第一步，向網路宣告新礦機加入
    const char* subscribe_msg = "{\"id\": 1, \"method\": \"mining.subscribe\", \"params\": [\"ManekiMiner/1.0\"]}\n";
    send(sock, subscribe_msg, strlen(subscribe_msg), 0);
    LOGI("已發送 mining.subscribe 指令");

    // 步驟 6：接收礦池回傳的伺服器資訊 (JSON 格式)
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_received > 0) {
        LOGI("收到礦池回覆：\n%s", buffer);
    } else {
        LOGE("未收到礦池回覆或連線已中斷");
    }

    // 測試完畢暫時關閉連線，下一階段再建立持續接收任務的無窮迴圈與授權驗證
    close(sock);
    LOGI("小菊完成連線測試，暫時進入休息狀態");
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string status = "小月與小菊引擎就緒。Stratum 網路模組已掛載。";
    return env->NewStringUTF(status.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_manekiminer_app_MainActivity_startMiningNative(JNIEnv *env, jobject thiz) {
    std::thread minerThread(startMiningLoop);
    minerThread.detach();
}