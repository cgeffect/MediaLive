#ifndef LOGUTILS_H_
#define LOGUTILS_H_

#include <list>
#include <mutex>
#include <cstdio>
#include <thread>
#include <cstring>
#include <cstdarg>
#include <sstream>
#include <unordered_map>

namespace mogic {
inline void PrintLog(const char format[], ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
    fprintf(stdout, "\n");
}

inline void PrintError(const char format[], ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

inline std::string formatHelper(const char format[], ...) {
    char buf[256];
    memset(buf, 0, 256);
    va_list args;
    va_start(args, format);
    vsnprintf(buf, 255, format, args);
    std::string ret(buf);
    va_end(args);

    return ret;
}

class MogicErrorTracker {
public:
    static MogicErrorTracker &GetInstance();
    ~MogicErrorTracker() = default;
    void recordError(const std::string &errorMessage);
    std::list<std::string> releaseStack();

private:
    static constexpr size_t maxNumOfRecords = 20;
    MogicErrorTracker() = default;
    static std::mutex mLocker;
    std::unordered_map<std::thread::id, std::list<std::string>> errorStack;
    std::unordered_map<std::thread::id, std::list<int>> errorCodeStack;
};

} // namespace mogic

#endif // LOGUTILS_H_
