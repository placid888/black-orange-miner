#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_manekiminer_app_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "黑橘雙貓挖礦引擎初始化完成";
    return env->NewStringUTF(hello.c_str());
}