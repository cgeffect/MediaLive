#include <atomic>
#include <csignal>
#include <iostream>
#include <thread>

#include "mk_mediakit.h"

namespace {
std::atomic<bool> g_running{true};

void HandleSignal(int) {
  g_running.store(false);
}
}  // namespace

int main(int argc, char** argv) {
  const char* ini_path = "./conf/config.ini";
  if (argc > 1 && argv[1] != nullptr) {
    ini_path = argv[1];
  }

  mk_env_init2(
      0,          // thread_num: auto
      0,          // log_level: trace
      LOG_CONSOLE | LOG_FILE,
      "./",      // log file path
      7,          // log keep days
      1,          // ini_is_path
      ini_path,
      1,          // ssl_is_path
      "./default.pem",
      nullptr);

  auto http_port = mk_http_server_start(8081, 0);
  auto rtmp_port = mk_rtmp_server_start(1935, 0);
  auto rtsp_port = mk_rtsp_server_start(5540, 0);

  if (!http_port || !rtmp_port || !rtsp_port) {
    std::cerr << "failed to start embedded media server, ports:"
              << " http=" << http_port
              << " rtmp=" << rtmp_port
              << " rtsp=" << rtsp_port << std::endl;
    mk_stop_all_server();
    return 1;
  }

  std::cout << "embedded media server started"
            << " | http=" << http_port
            << " | rtmp=" << rtmp_port
            << " | rtsp=" << rtsp_port << std::endl;

  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  while (g_running.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  mk_stop_all_server();
  std::cout << "embedded media server stopped" << std::endl;
  return 0;
}
