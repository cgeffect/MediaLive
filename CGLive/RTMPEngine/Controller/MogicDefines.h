//
// Created by haoyu on 2022/3/3.
//

#ifndef MOGIC_PAG_MOGICDEFINES_H
#define MOGIC_PAG_MOGICDEFINES_H

#include <cmath>
#include "LogUtils.h"

#define MOGIC_DEBUG

#define MOGIC_VERSION "v1.0.0"
#ifdef MOGIC_DEBUG
#define MOGIC_FFDECODE_THREAD_NUM 1
#else
#define MOGIC_FFDECODE_THREAD_NUM 4
#endif

#ifdef MOGIC_DEBUG
#define MOGIC_DLOG(...) mogic::PrintLog(__VA_ARGS__);
#else
#define MOGIC_DLOG(...)
#endif
#define MOGIC_INFO(...)
#define MOGIC_ERROR(...)
#define MOGIC_WARN(...)

#define MOGIC_CHECK_EQUAL(a, b, val) \
    if (a != b) return val;

#define MOGIC_CHECK_F_EQUAL(a, b, val) \
    if (std::fabs(a - b) > 1e-6) return val;

#define MOGIC_CHECK_RETURN(cond, val) \
    if (cond) {                       \
        return val;                   \
    }

#define MOGIC_CHECK_ELOG(cond, val, ...)                         \
    if (cond) {                                                  \
        std::string errMsg = mogic::formatHelper(__VA_ARGS__);   \
        MOGIC_ERROR(errMsg.c_str())                              \
        auto eTracker = mogic::MogicErrorTracker::GetInstance(); \
        std::string message;                                     \
        message += mogic::formatHelper("code: %d, msg: ", val);  \
        message += errMsg;                                       \
        eTracker.recordError(message);                           \
        return val;                                              \
    }

#define MOGIC_CHECK_ERROR(cond, val, ...)                        \
    if (cond) {                                                  \
        std::string errMsg = mogic::formatHelper(__VA_ARGS__);   \
        MOGIC_ERROR(errMsg.c_str())                              \
        auto eTracker = mogic::MogicErrorTracker::GetInstance(); \
        std::string message;                                     \
        message += mogic::formatHelper("code: %d, msg: ", val);  \
        message += errMsg;                                       \
        eTracker.recordError(message);                           \
    }

#define MOGIC_CHECK_DLOG(cond, val, ...) \
    if (cond) {                          \
        MOGIC_DLOG(__VA_ARGS__)          \
        return val;                      \
    }

#define MOGIC_CHECK_CONTINUE_DLOG(cond, val, ...) \
    if (cond) {                                   \
        MOGIC_DLOG(__VA_ARGS__)                   \
        continue;                                 \
    }

// ERROR CODES
#define MOGIC_MSG_OK 0
#define MOGIC_NULLPTR_ERROR -1
#define MOGIC_FILE_OPEN_ERROR -2
#define MOGIC_REQUEST_PARAM_ERROR -2
#define MOGIC_TEMPLATE_RENDER_ERROR -3
#define MOGIC_AUDIO_PROCESS_ERROR -4
#define MOGIC_AUDIO_MUX_ERROR -5
#define MOGIC_PAG_FILE_LOAD_ERROR -6
#define MOGIC_JSON_PARSE_ERROR -7
#define MOGIC_INVALID_TEMPLATE_PARAM -8
#define MOGIC_OSS_FETECH_ERROR -9
#define MOGIC_COVER_IMAGE_EXTRACT_ERROR -10
#define MOGIC_OSS_UPLOAD_ERROR -11
#define MOGIC_FFENCODER_INIT_ERROR -12
#define MOGIC_FFENCODER_ENCODE_ERROR -13
#define MOGIC_SKIP_RENDER_ON_INVALID_REQUEST -14

#define MOGIC_FFDECODER_INIT_ERROR -15
#define MOGIC_FFDECODER_SEEK_ERROR -16
#define MOGIC_FFDECODER_END_OF_FILE -17

#define MOGIC_MIX_CLIP_ERROR -20        //裁剪结束时间小于等于开始时间
#define MOGIC_MIX_TRACK_ERROR -21       //混音轨道不存在
#define MOGIC_DECODE_AUDIO_NO_FILE -22  //文件不存在
#define MOGIC_DECODE_AUDIO_NO_TRACK -23 //不存在音频轨道
#define MOGIC_DECODE_ERROR -24          //通用错误
#define MOGIC_ENCODE_ERROR -25          //通用错误

#endif // MOGIC_PAG_MOGICDEFINES_H
