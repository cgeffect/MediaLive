# 云端实时视频预览系统（mac 本地集成版）

按你的三模块拆分：

- `web`：浏览器实时拉流预览（flv.js）
- `src/encoder-pusher`：C++ 推流模块（streamId 任务管理 + 帧级渲染）
- `src/media-server`：内置流媒体服务器（C++ 程序 + 静态链接 ZLMediaKit mk_api）

另外：

- `src/third-party/ZLMediaKit`：你下载的 ZLMediaKit 源码
- `src/third-party/build.sh`：将 ZLMediaKit 编译并安装为静态库产物

## 目录

```text
.
├── web/
├── src/
│   ├── encoder-pusher/
│   │   ├── src/main.cpp
│   │   ├── CMakeLists.txt
│   │   ├── build.sh
│   │   ├── start_stream.sh
│   │   ├── stop_stream.sh
│   │   └── list_streams.sh
│   ├── media-server/
│   │   ├── src/main.cpp
│   │   ├── CMakeLists.txt
│   │   ├── build.sh
│   │   └── run_server.sh
└── scripts/
```

## 1) 依赖

```bash
brew install ffmpeg cmake git
```

## 2) 编译第三方静态库（ZLMediaKit）

```bash
bash src/third-party/build.sh
```

成功后生成：

- `src/third-party/install/zlm/lib/libmk_api.a`
- `src/third-party/install/zlm/include/*.h`

## 3) 默认构建方式（顶层 CMake）

```bash
cmake -S . -B build
cmake --build build -j "$(sysctl -n hw.logicalcpu)"
```

默认生成：

- `build/src/media-server/embedded_server`
- `build/src/encoder-pusher/encoder_pusher`

> `scripts/dev_up.sh` 默认会执行顶层构建流程。

## 3.1) 模块内构建（可选）

仅在单模块调试时使用：

```bash
bash src/media-server/build.sh
bash src/encoder-pusher/build.sh
```

## 4) 一键启动完整链路

```bash
bash scripts/dev_up.sh
```

顺序：

1. 启动 `embedded_server`（静态链接 mk_api）
2. 启动 `encoder_pusher`（通过 `start_stream.sh` 启动 `stream1`）
3. 启动 Web 页面（8080）

访问：

- 播放页：`http://127.0.0.1:8080`
- FLV 流：`http://127.0.0.1:8081/live/stream1.live.flv`

## 5) 停止

```bash
bash scripts/dev_down.sh
```

## 6) 单独启动/管理推流任务（统一 streamId 模式）

单流和多流都使用同一套命令：

```bash
bash src/encoder-pusher/start_stream.sh stream2
bash src/encoder-pusher/start_stream.sh stream3 --input /path/to/demo.mp4
bash src/encoder-pusher/list_streams.sh
bash src/encoder-pusher/stop_stream.sh stream2
bash src/encoder-pusher/stop_stream.sh stream3
```

拉流地址对应：

- `http://127.0.0.1:8081/live/stream2.live.flv`
- `http://127.0.0.1:8081/live/stream3.live.flv`

说明：

- `start_stream.sh`：启动指定 `streamId`（单流=只启动一个 streamId）
- `list_streams.sh`：查看所有流状态
- `stop_stream.sh`：停止单个流

## 7) 端口

- `1935`：RTMP 推流入口
- `8081`：HTTP-FLV 拉流
- `5540`：RTSP
- `8080`：本地 Web 页面

## 8) 当前“静态库+工程集成”说明

- 使用 `src/third-party/build.sh` 固化第三方构建参数
- 通过 `src/third-party/cmake/ZLMStatic.cmake` 导入 `ZLM::mk_api`
- `src/media-server/CMakeLists.txt` 直接链接该静态库
- `src/media-server/src/main.cpp` 以 C API 启动内置 HTTP/RTMP/RTSP

## 9) 当前“帧级渲染”能力说明

`src/encoder-pusher/src` 内已实现：

- `TaskManager`：按 `streamId` 管理任务对象
- 文件输入模式：`decode -> frame render -> encode -> rtmp push`
  - 解码：`libavcodec`
  - 帧处理：逐帧修改 Y 平面亮带（可替换成你业务特效）
  - 编码：H.264 (`libx264`)
  - 推流：FLV/RTMP (`libavformat`)
- 音频：当前对 AAC 轨做稳定透传
- 无输入文件模式：保留 testsrc/sine 命令行兜底，便于快速联调
