//
//  LiveProducterThread.cpp
//  FFmpegiOS
//
//  Created by Jason on 2021/4/13.
//  Copyright © 2021 Jason. All rights reserved.
//

#include "LiveProducterThread.h"

#include "LivePlatformCommon.h"

LiveProducterThread::LiveProducterThread() {
    pthread_mutex_init(&mLock, NULL);
    pthread_cond_init(&mCondition, NULL);
}

LiveProducterThread::~LiveProducterThread() {
}

void LiveProducterThread::start() {
    handleRun(NULL);
}

void LiveProducterThread::startAsync() {
    pthread_create(&mThread, NULL, startThread, this);
}

int LiveProducterThread::wait() {
    if (!mRunning) {
        printf("mRunning is false so return 0\n");
        return 0;
    }
    void *status;
    int ret = pthread_join(mThread, &status);
    printf("pthread_join thread return result is %d\n", ret);
    return ret;
}

void LiveProducterThread::stop() {
}

void* LiveProducterThread::startThread(void *ptr) {
    printf("starting thread\n");
    LiveProducterThread *thread = (LiveProducterThread *)ptr;
    thread->mRunning = true;
    thread->handleRun(ptr);
    thread->mRunning = false;
    return NULL;
}

void LiveProducterThread::waitOnNotify() {
    pthread_mutex_lock(&mLock);
    pthread_cond_wait(&mCondition, &mLock);
    pthread_mutex_unlock(&mLock);
}

void LiveProducterThread::notify() {
    pthread_mutex_lock(&mLock);
    pthread_cond_signal(&mCondition);
    pthread_mutex_unlock(&mLock);
}

void LiveProducterThread::handleRun(void *ptr) {
}
