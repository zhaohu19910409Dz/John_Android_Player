#include "MediaCore.h"

extern "C" JNIEXPORT jstring JNICALL  Java_com_john_testcplusplus_MediaPlayer_getPlayerVersion(JNIEnv *env,jobject /* this */)
{
    LOG("getPlayerVersion\r\n");
    std::string version = MediaCore::getInstance()->getVersion();;
    return env->NewStringUTF(version.c_str());
}

extern "C" JNIEXPORT void JNICALL  Java_com_john_testcplusplus_MediaPlayer_playVideo(JNIEnv *env,jobject obj,jstring file,jobject surface)
{
    string fileName = env->GetStringUTFChars(file,JNI_FALSE);
    LOG("fileName:%s\r\n",fileName.c_str());
    MediaCore::getInstance()->setFileName(fileName);

    ANativeWindow* pWindow = ANativeWindow_fromSurface(env,surface);
    MediaCore::getInstance()->setWindow(pWindow);

    MediaCore::getInstance()->InitFFmpeg();
    MediaCore::getInstance()->Start();
}
