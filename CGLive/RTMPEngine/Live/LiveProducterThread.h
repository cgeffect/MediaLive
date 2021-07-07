//
//  LiveProducterThread.h
//  FFmpegiOS
//
//  Created by Jason on 2021/4/13.
//  Copyright © 2021 Jason. All rights reserved.
//

#ifndef LiveProducterThread_h
#define LiveProducterThread_h

#include <pthread.h>

class LiveProducterThread {
public:
    LiveProducterThread();
    ~LiveProducterThread();
    
    void start();
    void startAsync();
    int wait();
    
    void waitOnNotify();
    void notify();
    virtual void stop();
    
protected:
    bool mRunning;
    virtual void handleRun(void *ptr);

protected:
    pthread_t mThread;
    pthread_mutex_t mLock;
    pthread_cond_t mCondition;
    
    static void* startThread(void *ptr);
};
#endif /* LiveProducterThread_h */
