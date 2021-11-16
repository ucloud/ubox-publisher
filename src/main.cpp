#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <termios.h>
#include "RemoteService.h"
#include "ControlHandler.h"
#include "tinylog/tlog.h"

void showUsage() {
    std::cout << "usage: publisher [-u unixsocket path] [-t logfile path]" << std::endl;
    std::cout << std::endl;
    std::cout << "-u the unixsocket path to control" << std::endl;
    std::cout << std::endl;
    std::cout << "-t output log file path" << std::endl;
    std::cout << std::endl;
    std::cout << "example: publisher -u @/tmp/publish.sock -t /var/log/publisher/video.log" << std::endl;
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN); 
    std::string sock_path = "@/tmp/publish.sock";
    std::string url = "";
    std::string logFile = "";
    std::string deviceName = "";
    int width = 1280;
    int height = 720;
    int fps = 30;
    int bitrate = 2000;
    signed char ch;

    if (argc < 2) {
        showUsage();
    }
    while ((ch = getopt(argc, argv, "u:t:d:a:w:h:f:b:")) != -1) {
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
            default:
                std::cout << "unknown option " << ch << std::endl;
                showUsage();
        }
    }

    if (logFile == "") {
        char log_file[128];
        int pos = sock_path.rfind('/');
        std::string log_name = sock_path.substr(pos+1, sock_path.length()-pos);
        snprintf(log_file, 128, "log/publisher/%s.log", log_name.c_str());
        logFile = log_file;
    }
    tlog_init(logFile.c_str(), 100*1024 * 1024, 10, 0, 0);

    tlog(TLOG_INFO, "control bind address : %s\n", sock_path.c_str());
    tlog(TLOG_INFO, "!!!program start!!!\n");

    ControlHandler handler;
    RemoteService service(sock_path.c_str(), handler);
    std::thread controlThread(&RemoteService::run, &service);

    // for test
    if (deviceName != "" && url != "") {
        tlog(TLOG_INFO, "for test : (device) %s, (url) %s\n", deviceName.c_str(), url.c_str());
    	handler.StartStream(deviceName.c_str(), width, height, 640, 480, fps, bitrate, url.c_str());
    }

    while(true) {
        struct termios oldt, newt;
        tcgetattr( STDIN_FILENO, &oldt );
        newt = oldt;
        newt.c_lflag &= ~ICANON;
        newt.c_lflag &= ~ECHO;
        newt.c_cc[VMIN] = 1;
        newt.c_cc[VTIME] = 0;
        tcsetattr( STDIN_FILENO, TCSANOW, &newt );
        ch = getchar();
        tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
        if (ch == 'q') {
            service.quit();
            controlThread.join();
            tlog(TLOG_INFO, "control thread quit!\n");
            break;
        }
    }

    tlog(TLOG_INFO, "!!!program end!!!\n");
    return 0;
}
