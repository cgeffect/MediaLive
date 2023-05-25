//
//  LivePush.hpp
//  CGLive
//
//  Created by Jason on 2022/8/22.
//

#ifndef LivePush_hpp
#define LivePush_hpp

#include <stdio.h>
namespace Live {
class LivePush {
public:
    LivePush() = default;
    ~LivePush();
    
    int pushUsePacket(const char *file);
    int pushUseWrite(const char *file);

};
}
#endif /* LivePush_hpp */
