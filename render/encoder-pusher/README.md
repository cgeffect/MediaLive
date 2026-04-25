# encoder-pusher 模块说明

本目录是渲染推流模块，目标是把输入媒体按 `streamId` 处理后推到内置流媒体服务（ZLMediaKit）。

当前代码已按 `app / core / pipeline` 分层，便于后续扩展多种推流后端和渲染能力。

## 目录结构

```text
render/encoder-pusher/
├── build.sh
├── push_stream.sh
├── start_stream.sh
├── stop_stream.sh
├── list_streams.sh
├── CMakeLists.txt
└── src/
    ├── app/
    │   └── main.cpp
    ├── core/
    │   ├── task_config.h
    │   ├── task_config.cpp
    │   ├── task_manager.h
    │   └── task_manager.cpp
    └── pipeline/
        ├── frame_render_pusher.h
        ├── frame_render_pusher.cpp
        ├── frame_renderer.h
        ├── frame_renderer.cpp
        ├── pipeline_context.h
        ├── pipeline_context.cpp
        ├── ffmpeg_utils.h
        ├── ffmpeg_utils.cpp
        ├── input_initializer.h
        ├── input_initializer.cpp
        ├── output_initializer.h
        ├── output_initializer.cpp
        ├── video_processor.h
        ├── video_processor.cpp
        ├── audio_processor.h
        └── audio_processor.cpp
```

## 顶层文件职责

- 默认构建方式
  - 从项目根目录执行：`cmake -S . -B build && cmake --build build -j ...`
  - 产物路径：`build/render/encoder-pusher/encoder_pusher`

- `build.sh`
  - 模块内单独构建入口（可选，主要用于局部调试）。

- `push_stream.sh`
  - 启动默认推流任务（单实例入口），自动触发编译。

- `start_stream.sh`
  - 按 `streamId` 启动独立推流任务并记录 PID/日志到 `state/`。

- `stop_stream.sh`
  - 停止指定 `streamId` 对应任务。

- `list_streams.sh`
  - 列出当前 `streamId` 任务运行状态。

- `CMakeLists.txt`
  - 定义源码编译目标及 FFmpeg 依赖链接。

## src/app

- `app/main.cpp`
  - 程序入口。
  - 负责：信号注册（优雅退出）、参数解析调用、启动 `TaskManager`。
  - 不放业务细节，保持入口简洁。

## src/core

- `core/task_config.h`
  - 声明 `TaskConfig`，定义单个推流任务所需参数。

- `core/task_config.cpp`
  - 实现命令行参数解析 `ParseArgs()`。
  - 实现目标地址拼装 `BuildTargetUrl()`。
  - 提供使用说明 `UsageText()`。

- `core/task_manager.h`
  - 声明 `TaskManager` 与任务状态结构。
  - 负责多 `streamId` 的任务生命周期管理接口。

- `core/task_manager.cpp`
  - 实现任务启动、停止、列表查询。
  - 将每个任务绑定到一个工作线程并调用 pipeline 执行。

## src/pipeline

- `pipeline/frame_render_pusher.h`
  - 声明推流执行器 `FrameRenderPusher`。

- `pipeline/frame_render_pusher.cpp`
  - pipeline 编排层（orchestration）。
  - 负责串联输入初始化、输出初始化、视频处理、音频处理、收尾 flush。
  - 保留无输入时的 CLI fallback 推流。

- `pipeline/frame_renderer.h`
  - 声明帧级渲染器接口。

- `pipeline/frame_renderer.cpp`
  - 实现逐帧像素处理逻辑（当前示例是 Y 平面移动亮带）。
  - 后续可替换为真实滤镜、贴图、字幕、转场等效果。

- `pipeline/pipeline_context.h`
  - 声明 `ApiPipeline`，集中持有 FFmpeg 上下文、流索引、帧/包对象、时间基状态。

- `pipeline/pipeline_context.cpp`
  - 实现 `ApiPipeline` 资源回收逻辑（RAII）。

- `pipeline/ffmpeg_utils.h`
  - FFmpeg 通用工具函数声明。

- `pipeline/ffmpeg_utils.cpp`
  - `AvErr2Str()`：错误码转字符串。
  - `PaceByTimestamp()`：按时间戳做实时节奏控制。

- `pipeline/input_initializer.h`
  - 声明输入初始化接口。

- `pipeline/input_initializer.cpp`
  - 打开输入媒体、查找流信息、初始化视频解码器。

- `pipeline/output_initializer.h`
  - 声明输出初始化接口。

- `pipeline/output_initializer.cpp`
  - 初始化输出 FLV/RTMP、视频编码器、可选音频输出流。
  - 分配帧/包缓存并写输出头。

- `pipeline/video_processor.h`
  - 声明视频处理流程接口。

- `pipeline/video_processor.cpp`
  - 实现视频主链路：
    - 解码输入视频帧
    - 格式转换（必要时）
    - 帧级渲染处理
    - 重新编码并写入输出
  - 实现编码器 flush 收尾。

- `pipeline/audio_processor.h`
  - 声明音频处理接口。

- `pipeline/audio_processor.cpp`
  - 实现音频透传写出（当前为 AAC copy 模式）。

## 数据流总览

1. `main.cpp` 解析参数，交给 `TaskManager`。
2. `TaskManager` 为每个 `streamId` 启动任务线程。
3. `FrameRenderPusher` 组织 pipeline：
   - `InitInput` -> `InitOutput`
   - 循环读取包
   - 视频包走 `video_processor`（解码/渲染/编码）
   - 音频包走 `audio_processor`（透传）
4. 推流到 `rtmp://<host>:<port>/<app>/<streamId>`。

## 后续建议

- 将 `audio_processor` 从透传升级为“解码/混音/重采样/编码”。
- 在 `frame_renderer` 中引入可配置特效链（配置驱动）。
- 为 pipeline 增加统计与观测（FPS、延迟、丢帧、重连次数）。
