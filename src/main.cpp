#include "ControlHandler.h"
#include "RemoteService.h"
#include "tinylog/tlog.h"
#include <iostream>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <thread>
#include <unistd.h>

void showUsage() {
  std::cout << "usage: publisher [-u @/tmp/publish.sock] [-t logfile path] [-s "
               "v4l2] [-d input]"
               "[-c] [-f 24] [-w 640] [-h 480] [-b 2000] [-p intel|jetson][-a "
               "rtmp://x.x.x] [-v]"
            << std::endl;
  std::cout << "    -s input source type, rtsp|v4l2|fpga-wrh" << std::endl
            << "    -c stream copy" << std::endl
            << "    -b bitrate, Kbps" << std::endl
            << "    -v verbose" << std::endl;
  std::cout << "example: publisher -u @/tmp/publish.sock -t "
               "/var/log/publisher/video.log"
            << std::endl;
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
  signal(SIGPIPE, SIG_IGN);
  std::string sock_path = "@/tmp/publish.sock";
  std::string url = "";
  std::string logFile = "";
  std::string deviceName = "";
  int width = 0;
  int height = 0;
  int fps = 0;
  int bitrate = 2000;
  bool copy = false;
  bool verbose = false;
  signed char ch;
  std::string type, accelPlatform;

  if (argc < 2) {
    showUsage();
  }
  while ((ch = getopt(argc, argv, "u:t:s:d:a:w:h:f:b:p:cv")) != -1) {
    switch (ch) {
    case 'u':
      sock_path = optarg;
      break;
    case 't':
      logFile = optarg;
      break;
    case 'd':
      deviceName = optarg;
      break;
    case 'p':
      accelPlatform = optarg;
      break;
    case 'a':
      url = optarg;
      break;
    case 'w':
      width = atoi(optarg);
      break;
    case 'h':
      height = atoi(optarg);
      break;
    case 'f':
      fps = atoi(optarg);
      break;
    case 'b':
      bitrate = atoi(optarg);
      break;
    case 's':
      type = optarg;
      break;
    case 'c':
      copy = true;
      break;
    case 'v':
      verbose = true;
      break;
    default:
      std::cout << "unknown option " << ch << std::endl;
    }
  }

  // tlog init
  if (logFile == "") {
    char log_file[128];
    int pos = sock_path.rfind('/');
    std::string log_name = sock_path.substr(pos + 1, sock_path.length() - pos);
    snprintf(log_file, 128, "log/publisher/%s.log", log_name.c_str());
    logFile = log_file;
  }
  tlog_init(logFile.c_str(), 100 * 1024 * 1024, 10, 0, 0);
  if (verbose)
    tlog_setlevel(TLOG_DEBUG);

  tlog(TLOG_INFO, "control bind address : %s\n", sock_path.c_str());
  tlog(TLOG_INFO, "!!!program start!!!\n");

  ControlHandler handler;
  RemoteService service(sock_path.c_str(), handler);
  std::thread controlThread(&RemoteService::run, &service);

  // for test
  if (deviceName != "" && url != "") {
    tlog(TLOG_INFO, "(device) %s, (accel) %s, (url) %s\n", deviceName.c_str(),
         accelPlatform.c_str(), url.c_str());
    handler.StartStream(type.c_str(), deviceName.c_str(), accelPlatform.c_str(),
                        width, height, copy, 640, 480, fps, bitrate,
                        url.c_str());
  }

  while (true) {
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~ICANON;
    newt.c_lflag &= ~ECHO;
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    if (ch == 'q') {
      service.quit();
      controlThread.join();
      tlog(TLOG_INFO, "control thread quit!\n");
      break;
    }
    sleep(1);
  }

  tlog(TLOG_INFO, "!!!program end!!!\n");
  return 0;
}
