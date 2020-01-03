//
// Created by hu zhao on 2019-12-30.
//

#include "JQueue.h"
#include "pthread.h"
#include "unistd.h"

template <class T>
JQueue<T>::JQueue(int size):max_size(0),rIndex(0),wIndex(0),size(0),rindex_shown(0),keep_last(0)
{
    pthread_mutex_init(&mutex_id, NULL);
    pthread_cond_init(&consume_condition_id, NULL);
    max_size = MIN(size, FRAME_QUEUE_SIZE);
}

template <class T>
JQueue<T>::JQueue(int size, char* name):max_size(0),rIndex(0),wIndex(0),size(0),rindex_shown(0),keep_last(0),pName(NULL)
{
    pthread_mutex_init(&mutex_id, NULL);
    pthread_cond_init(&consume_condition_id, NULL);
    max_size = MIN(size, FRAME_QUEUE_SIZE);

    int len = strlen(name);
    pName = (char*)malloc(len+1);
    memcpy(pName, name, len);
    pName[len] = '\0';
    LOG("Create JQueue:%s\r\n",name);
}
template <class T>
JQueue<T>::~JQueue()
{
    pthread_mutex_destroy(&mutex_id);
    pthread_cond_destroy(&consume_condition_id);
    if(pName)
    {
        delete pName;
        pName = NULL;
    }
}

template <class T>
T* JQueue<T>::queue_peek()
{
    return &data[(rIndex + rindex_shown) % max_size];
}

template <class T>
T*  JQueue<T>::queue_peek_next()
{
    return &data[(rIndex + rindex_shown + 1) % max_size];
}

template <class T>
T*  JQueue<T>::queue_peek_last()
{
    return &data[rIndex];
}

template <class T>
T* JQueue<T>::queue_peek_writable()
{
    pthread_mutex_lock(&mutex_id);
    while(size >= max_size)
    {
        //LOG("name:%s,pthread_cond_wait\r\n",pName);
        pthread_cond_wait(&consume_condition_id, &mutex_id);
    }
    //LOG("name:%s,queue_peek_writable\r\n",pName);
    pthread_mutex_unlock(&mutex_id);
    return &data[wIndex];
}

template <class T>
T*  JQueue<T>::queue_peek_readable()
{
    pthread_mutex_lock(&mutex_id);
    while(size - rindex_shown <= 0)
    {
        //LOG("name:%s,pthread_cond_wait\r\n",pName);
        pthread_cond_wait(&consume_condition_id, &mutex_id);
    }
    //LOG("name:%s,queue_peek_readable\r\n",pName);
    pthread_mutex_unlock(&mutex_id);
    return &data[(rIndex + rindex_shown) % max_size];
}

template <class T>
void  JQueue<T>::queue_push()
{
    if(++wIndex == max_size)
        wIndex = 0;
    pthread_mutex_lock(&mutex_id);
    //LOG("name:%s,queue_push\r\n",pName);
    size++;
    pthread_cond_signal(&consume_condition_id);
    pthread_mutex_unlock(&mutex_id);
}

template <class T>
void  JQueue<T>::queue_next()
{
    if(keep_last && !rindex_shown)
    {
        rindex_shown = 1;
        return;
    }
    if(++rIndex == max_size)
        rIndex = 0;
    pthread_mutex_lock(&mutex_id);
    //LOG("name:%s,queue_next\r\n",pName);
    size--;
    pthread_cond_signal(&consume_condition_id);
    pthread_mutex_unlock(&mutex_id);
}

template <class T>
int  JQueue<T>::queuen_nb_remaining()
{
    return size - rindex_shown;
}