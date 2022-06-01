#include "LogUtils.h"

namespace mogic {
std::mutex MogicErrorTracker::mLocker = {};

MogicErrorTracker &MogicErrorTracker::GetInstance() {
    static MogicErrorTracker mogicErrorTracker = {};
    return mogicErrorTracker;
}

void MogicErrorTracker::recordError(const std::string &errorMessage) {
    // allow single write
    auto mThreadId = std::this_thread::get_id();
    std::lock_guard<std::mutex> autoLock(mLocker);
    errorStack[mThreadId].push_back(errorMessage);
    while (errorStack[mThreadId].size() > maxNumOfRecords) {
        errorStack[mThreadId].pop_front();
    }
}

std::list<std::string> MogicErrorTracker::releaseStack() {
    // unnecessary to obtain lock
    auto mThreadId = std::this_thread::get_id();
    auto ret = errorStack[mThreadId];
    errorStack.erase(mThreadId);
    return ret;
}

} // namespace mogic
