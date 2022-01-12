#ifndef _CONTROLSERVICEUN_H_20210902_
#define _CONTROLSERVICEUN_H_20210902_

#define MESSAGE_MAX_SIZE 8192
#define MESSAGE_HEADER_SIZE 8

#include <map>
#include <string>
#include "ControlHandler.h"

struct cJSON;

class UnixDomainService {
public:
    UnixDomainService(ControlHandler& stream);
    ~UnixDomainService();

    int Open(const char* path);
    int Close();
    void Wait();
private:
    cJSON* SetBitrate_Parse(cJSON*);
    cJSON* GetBitrate_Parse(cJSON*);
    cJSON* PushStream_Parse(cJSON*);
    cJSON* CloseStream_Parse(cJSON*);

    cJSON* UnPacket(unsigned char* buf, int size);
    cJSON* GetJsonByCode(int retCode, const char* msg);
    int Packet(unsigned char* buf, int bufSize, cJSON* jsonData);
    int PacketErrorMsg(unsigned  char* buf, int bufSize , int retCode, const char* msg);


private:
    int srvFD;
    bool opened;
    ControlHandler& handler;

    typedef cJSON* (UnixDomainService::* CommandFunc)(cJSON*);
    typedef std::map<std::string, CommandFunc>  CommandFuncMap;
    CommandFuncMap funcMap;
};

#endif//_CONTROLSERVICEUN_H_20210902_
