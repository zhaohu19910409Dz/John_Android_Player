//
// Created by hu zhao on 2019-12-30.
//

#ifndef TESTCPLUSPLUS_JQUEUE_H
#define TESTCPLUSPLUS_JQUEUE_H

#include <unistd.h>
#include <pthread.h>
#include <jni.h>
#include <android/log.h>
#define LOG(...) __android_log_print(ANDROID_LOG_INFO,"MediaCore",__VA_ARGS__)

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE   16
#define SAMPLE_QUEUE_SIZE       9
#define FRAME_QUEUE_SIZE    MAX(SAMPLE_QUEUE_SIZE, MAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))
template <class T>

class JQueue {
private:
    char* pName;
    T data[FRAME_QUEUE_SIZE];
    int rIndex;
    int wIndex;
    int size;
    int max_size;
    int keep_last;
    int rindex_shown;
    pthread_mutex_t mutex_id;
    pthread_cond_t consume_condition_id;
public:
    JQueue(int size);
    JQueue(int size, char* name);
    ~JQueue();

    //get need render data
    T* queue_peek();
    T* queue_peek_next();
    T* queue_peek_last();
    T* queue_peek_writable();
    T* queue_peek_readable();
    void queue_push();
    void queue_next();
    int queuen_nb_remaining();
};


#endif //TESTCPLUSPLUS_JQUEUE_H
