#include "RemoteService.h"
#include "UnixDomainService.h"
#include "tinylog/tlog.h"

RemoteService::RemoteService(const char* path, ControlHandler& handler) :handler(handler){
    if (path[0] != '@') {
        this->path = "@";
    }
    this->path.append(path);
    rpc_server = NULL;
}

RemoteService::~RemoteService() {
    if (rpc_server != NULL)
        ((UnixDomainService*)rpc_server)->Close();
}

void RemoteService::run(){
    tlog(TLOG_INFO, "adjust bitrate is running, address %s\n", path.c_str());

    UnixDomainService serviceUn(handler);
    if (serviceUn.Open(path.c_str()) != 0) {
        tlog(TLOG_INFO, "control service open failed\n");
        return;
    }
    rpc_server = (void*)&serviceUn;
    serviceUn.Wait();
    serviceUn.Close();
    tlog(TLOG_INFO, "adjust bitrate is stopped\n");
}

void RemoteService::quit() {
    /*
    (*(std::unique_ptr<grpc::Server> *)rpc_server)->Shutdown();
     */
    if (rpc_server != NULL)
        ((UnixDomainService*)rpc_server)->Close();
}
