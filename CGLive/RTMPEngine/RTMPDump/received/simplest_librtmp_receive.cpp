/**
 * Simplest Librtmp Receive
 *
 * 雷霄骅，张晖
 * leixiaohua1020@126.com
 * zhanghuicuc@gmail.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * 本程序用于接收RTMP流媒体并在本地保存成FLV格式的文件。
 * This program can receive rtmp live stream and save it as local flv file.
 */
#include <stdio.h>
#include "rtmplog.h"
#include "rtmp.h"
#include <memory>

#include "simplest_librtmp_receive.h"

namespace rtmp {

int received_flv(char* argv) {

//    double duration = -1;
    int nRead;
    //is live stream ?
    bool bLiveStream=true;


    int bufsize=1024*1024*10;
    char *buf = (char*)malloc(bufsize);
    memset(buf,0,bufsize);
    long countbufsize=0;

    FILE *fp = fopen(argv,"wb");
    if (!fp){
        printf("Open File Error.\n");
        return -1;
    }

    /* set log level */
    //RTMP_LogLevel loglvl=RTMP_LOGDEBUG;
    //RTMP_LogSetLevel(loglvl);

    RTMP *rtmp=RTMP_Alloc();
    RTMP_Init(rtmp);
    //set connection timeout,default 30s
    rtmp->Link.timeout=10;
    // HKS's live URL
    if(!RTMP_SetupURL(rtmp,(char *)"rtmp://172.16.184.26:1935/live/livestream"))
    {
        printf("SetupURL Err\n");
        RTMP_Free(rtmp);
        return -1;
    }
    if (bLiveStream){
        rtmp->Link.lFlags|=RTMP_LF_LIVE;
    }

    //1hour
    RTMP_SetBufferMS(rtmp, 3600*1000);

    if(!RTMP_Connect(rtmp,NULL)){
        printf("Connect Err\n");
        RTMP_Free(rtmp);
        return -1;
    }

    if(!RTMP_ConnectStream(rtmp,0)){
        printf("ConnectStream Err\n");
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        return -1;
    }

    while(1){
        nRead = RTMP_Read(rtmp,buf,bufsize);
        if (nRead <= 0) {
            break;;
        }
        fwrite(buf,1,nRead,fp);

        countbufsize+=nRead;
        printf("Receive: %5dByte, Total: %5.2fkB\n",nRead,countbufsize*1.0/1024);
    }

    if(fp)
        fclose(fp);

    if(buf){
        free(buf);
    }

    if(rtmp){
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        rtmp=NULL;
    }
    return 0;
}

}

int received_flv1(char* argv) {
    rtmp::received_flv(argv);
    return 0;
}
