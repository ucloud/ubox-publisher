#ifndef _ADJUSTBITRATE_H_20210819_
#define _ADJUSTBITRATE_H_20210819_

#include <string>
#include "ControlHandler.h"

class RemoteService{
public:
    RemoteService(const char* path, ControlHandler& handler);
    ~RemoteService();

    void run();
    void quit();

private:
    std::string path;
    ControlHandler& handler;
    void* rpc_server;
};

#endif//_ADJUSTBITRATE_H_20210819_
