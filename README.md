# 云端实时视频预览系统（mac 本地集成版）

按你的三模块拆分：

- `web`：浏览器实时拉流预览（flv.js）
- `render/encoder-pusher`：C++ 推流模块（streamId 任务管理 + 帧级渲染）
- `render/media-server`：内置流媒体服务器（C++ 程序 + 静态链接 ZLMediaKit mk_api）

另外：

- `render/third-party/ZLMediaKit`：你下载的 ZLMediaKit 源码
- `render/third-party/build.sh`：将 ZLMediaKit 编译并安装为静态库产物

## 目录

```text
.
├── web/
├── render/
│   ├── encoder-pusher/
│   │   ├── src/main.cpp
│   │   ├── CMakeLists.txt
│   │   ├── build.sh
│   │   ├── push_stream.sh
│   │   ├── start_stream.sh
│   │   ├── stop_stream.sh
│   │   └── list_streams.sh
│   ├── media-server/
│   │   ├── src/main.cpp
│   │   ├── CMakeLists.txt
│   │   ├── build.sh
│   │   └── run_server.sh
│   └── third-party/
│       ├── ZLMediaKit/
│       ├── build.sh
│       ├── install/zlm/
│       └── cmake/ZLMStatic.cmake
└── scripts/
```

## 1) 依赖

```bash
brew install ffmpeg cmake git
```

## 2) 编译第三方静态库（ZLMediaKit）

```bash
bash render/third-party/build.sh
```

成功后生成：

- `render/third-party/install/zlm/lib/libmk_api.a`
- `render/third-party/install/zlm/include/*.h`

## 3) 默认构建方式（顶层 CMake）

```bash
cmake -S . -B build
cmake --build build -j "$(sysctl -n hw.logicalcpu)"
```

默认生成：

- `build/render/media-server/embedded_server`
- `build/render/encoder-pusher/encoder_pusher`

> `scripts/dev_up.sh` 默认会执行顶层构建流程。

## 3.1) 模块内构建（可选）

仅在单模块调试时使用：

```bash
bash render/media-server/build.sh
bash render/encoder-pusher/build.sh
```

## 4) 一键启动完整链路

```bash
bash scripts/dev_up.sh
```

顺序：

1. 启动 `embedded_server`（静态链接 mk_api）
2. 启动 `encoder_pusher`（默认 `streamId=stream1`）
3. 启动 Web 页面（8080）

访问：

- 播放页：`http://127.0.0.1:8080`
- FLV 流：`http://127.0.0.1:8081/live/stream1.live.flv`

## 5) 停止

```bash
bash scripts/dev_down.sh
```

## 6) 单独启动推流任务（可指定 streamId）

默认任务：

```bash
bash render/encoder-pusher/push_stream.sh --stream-id stream2
```

多流并发（推荐用管理脚本）：

```bash
bash render/encoder-pusher/start_stream.sh stream2
bash render/encoder-pusher/start_stream.sh stream3 --input /path/to/demo.mp4
bash render/encoder-pusher/list_streams.sh
bash render/encoder-pusher/stop_stream.sh stream2
bash render/encoder-pusher/stop_stream.sh stream3
```

拉流地址对应：

- `http://127.0.0.1:8081/live/stream2.live.flv`
- `http://127.0.0.1:8081/live/stream3.live.flv`

## 7) 端口

- `1935`：RTMP 推流入口
- `8081`：HTTP-FLV 拉流
- `5540`：RTSP
- `8080`：本地 Web 页面

## 8) 当前“静态库+工程集成”说明

- 使用 `render/third-party/build.sh` 固化第三方构建参数
- 通过 `render/third-party/cmake/ZLMStatic.cmake` 导入 `ZLM::mk_api`
- `render/media-server/CMakeLists.txt` 直接链接该静态库
- `render/media-server/src/main.cpp` 以 C API 启动内置 HTTP/RTMP/RTSP

## 9) 当前“帧级渲染”能力说明

`render/encoder-pusher/src` 内已实现：

- `TaskManager`：按 `streamId` 管理任务对象
- 文件输入模式：`decode -> frame render -> encode -> rtmp push`
  - 解码：`libavcodec`
  - 帧处理：逐帧修改 Y 平面亮带（可替换成你业务特效）
  - 编码：H.264 (`libx264`)
  - 推流：FLV/RTMP (`libavformat`)
- 音频：当前对 AAC 轨做稳定透传
- 无输入文件模式：保留 testsrc/sine 命令行兜底，便于快速联调

下一步你可以继续把“亮带效果”替换成多轨合成、滤镜链、字幕叠加、转场等真实渲染逻辑。



bash render/encoder-pusher/push_stream.sh --stream-id stream1 --input /绝对路径/xxx.mp4

bash render/encoder-pusher/start_stream.sh stream1 --input /绝对路径/xxx.mp4