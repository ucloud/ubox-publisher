#include "UnixDomainService.h"
#include "tinylog/tlog.h"
#include <arpa/inet.h>
#include <cjson/cJSON.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define RETCODE_TYPE_SUCCESS 0
#define RETCODE_TYPE_DEVICE_NOTOPEN 1
#define RETCODE_TYPE_RTMPSERVER_DISABLE 2
#define RETCODE_TYPE_WRONG_DATA 3
#define RETCODE_TYPE_VALUE_INVALID 4
#define RETCODE_TYPE_UNKNOWN_ERROR 5

UnixDomainService::UnixDomainService(ControlHandler &handler)
    : handler(handler) {
  srvFD = -1;
  opened = false;

  funcMap["SetBitrate"] = &UnixDomainService::SetBitrate_Parse;
  funcMap["GetBitrate"] = &UnixDomainService::GetBitrate_Parse;
  funcMap["PushStream"] = &UnixDomainService::PushStream_Parse;
  funcMap["CloseStream"] = &UnixDomainService::CloseStream_Parse;
}

UnixDomainService::~UnixDomainService() {
  if (srvFD != -1) {
    close(srvFD);
    srvFD = -1;
  }
  opened = false;
}

int UnixDomainService::Open(const char *path) {
  if (opened) {
    if (srvFD != -1) {
      close(srvFD);
      srvFD = -1;
    }
    opened = false;
  }
  struct sockaddr_un srv_addr;
  socklen_t srv_addr_len;

  // init server socket
  if ((srvFD = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
    tlog(TLOG_INFO, "socket error");
    return 0;
  }

  memset(&srv_addr, 0, sizeof(srv_addr));
  srv_addr.sun_family = AF_UNIX;
  memcpy(srv_addr.sun_path, path, strlen(path));
  srv_addr.sun_path[0] = 0;
  srv_addr_len = sizeof(srv_addr.sun_family) + strlen(path);
  ;

  if (bind(srvFD, (struct sockaddr *)&srv_addr, srv_addr_len) < 0) {
    tlog(TLOG_INFO, "bind error");
    close(srvFD);
    srvFD = -1;
    opened = false;
    return -1;
  }
  opened = true;

  tlog(TLOG_INFO, "%s, unix domain server open", __FUNCTION__);
  return 0;
}

void UnixDomainService::Wait() {
  if (!opened || srvFD == -1) {
    return;
  }
  int dataSize = 0;
  int sendSize = 0;
  int returnSize = 0;
  unsigned char buf[MESSAGE_MAX_SIZE];
  struct sockaddr_un fromAddr;
  socklen_t fromLength = sizeof(fromAddr);
  while (true) {
    dataSize = recvfrom(srvFD, buf, MESSAGE_MAX_SIZE, 0,
                        (struct sockaddr *)&fromAddr, &fromLength);
    // tlog(TLOG_INFO,  "unix domain socket read data %d", dataSize);
    if (dataSize <= 0) {
      tlog(TLOG_INFO, "read data error: %d", dataSize);
      break;
    }
    // unpacket data
    cJSON *json = UnPacket(buf, dataSize);
    if (json == NULL) {
      sendSize = PacketErrorMsg(buf, MESSAGE_MAX_SIZE, RETCODE_TYPE_WRONG_DATA,
                                "can not parse data");
      returnSize = sendto(srvFD, buf, sendSize, 0, (struct sockaddr *)&fromAddr,
                          fromLength);
      if (returnSize < 0) {
        tlog(TLOG_INFO, "send data error: %d from %s", returnSize,
             fromAddr.sun_path + 1);
        break;
      }
      tlog(TLOG_INFO, "send data: %s", "can not parse data");
      continue;
    }

    // parse action type
    cJSON *jsonCommand = cJSON_GetObjectItem(json, "Action");
    if (!cJSON_IsString(jsonCommand)) {
      tlog(TLOG_INFO, "wrong data, data no action");
      cJSON_Delete(json);

      sendSize = PacketErrorMsg(buf, MESSAGE_MAX_SIZE, RETCODE_TYPE_WRONG_DATA,
                                "data no action");
      returnSize = sendto(srvFD, buf, sendSize, 0, (struct sockaddr *)&fromAddr,
                          fromLength);
      if (returnSize < 0) {
        tlog(TLOG_INFO, "send data error: %d", returnSize);
      }
      tlog(TLOG_INFO, "send data: %s", "data no action");
      continue;
    }

    CommandFuncMap::iterator it = funcMap.find(jsonCommand->valuestring);
    if (it == funcMap.end()) {
      tlog(TLOG_INFO, "wrong data, unknown action");
      sendSize = PacketErrorMsg(buf, MESSAGE_MAX_SIZE, RETCODE_TYPE_WRONG_DATA,
                                "unknown action");
    } else {
      // parse other data
      cJSON *jsonReturn = (this->*it->second)(json);
      if (jsonReturn != NULL) {
        sendSize = Packet(buf, MESSAGE_MAX_SIZE, jsonReturn);
        cJSON_Delete(jsonReturn);
      } else {
        tlog(TLOG_INFO, "no return, unknown error");
        sendSize = PacketErrorMsg(buf, MESSAGE_MAX_SIZE,
                                  RETCODE_TYPE_UNKNOWN_ERROR, "unknown error");
      }
    }

    cJSON_Delete(json);
    buf[sendSize] = 0;
    returnSize = sendto(srvFD, buf, sendSize, 0, (struct sockaddr *)&fromAddr,
                        fromLength);
    if (returnSize < 0) {
      tlog(TLOG_INFO, "send data error: %d", returnSize);
    }
    tlog(TLOG_INFO, "send response: %s", buf + 4 + sizeof(int));
  }
}

int UnixDomainService::Close() {
  tlog(TLOG_INFO, "%s, unix domain server close", __FUNCTION__);
  if (srvFD != -1) {
    shutdown(srvFD, SHUT_RDWR);
    close(srvFD);
    srvFD = -1;
  }
  opened = false;
  return 0;
}

cJSON *UnixDomainService::SetBitrate_Parse(cJSON *jsonRequest) {
  cJSON *jsonBitrate = cJSON_GetObjectItem(jsonRequest, "Bitrate");
  if (!cJSON_IsNumber(jsonBitrate)) {
    tlog(TLOG_INFO, "wrong data, data no bitrate");
    return NULL;
  }
  // tlog(TLOG_INFO, "request: bitrate %d   response: 0",
  // jsonBitrate->valueint);

  if (jsonBitrate->valueint > 100000 || jsonBitrate->valueint < 1) {
    return GetJsonByCode(RETCODE_TYPE_VALUE_INVALID,
                         "bitrate value is invalid");
  }
  handler.SetBitrate(NULL, jsonBitrate->valueint);
  return GetJsonByCode(RETCODE_TYPE_SUCCESS, "success");
}

cJSON *UnixDomainService::GetBitrate_Parse(cJSON *jsonRequest) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "RetCode", RETCODE_TYPE_SUCCESS);
  cJSON_AddStringToObject(json, "Message", "success");
  cJSON_AddNumberToObject(json, "Bitrate", handler.GetBitrate(NULL));
  return json;
}

cJSON *UnixDomainService::PushStream_Parse(cJSON *jsonRequest) {
  tlog(TLOG_INFO, "%s, push stream:\"%s\"", __FUNCTION__,
       cJSON_Print(jsonRequest));

  cJSON *jsonInputType = cJSON_GetObjectItem(jsonRequest, "InputType");
  cJSON *jsonDevice = cJSON_GetObjectItem(jsonRequest, "Device");
  cJSON *jsonAccel = cJSON_GetObjectItem(jsonRequest, "Accel");
  cJSON *jsonSrcWidth = cJSON_GetObjectItem(jsonRequest, "SrcWidth");
  cJSON *jsonSrcHeight = cJSON_GetObjectItem(jsonRequest, "SrcHeight");
  cJSON *jsonStreamCopy = cJSON_GetObjectItem(jsonRequest, "HevcEncode");
  cJSON *jsonDstWidth = cJSON_GetObjectItem(jsonRequest, "DstWidth");
  cJSON *jsonDstHeight = cJSON_GetObjectItem(jsonRequest, "DstHeight");
  cJSON *jsonURL = cJSON_GetObjectItem(jsonRequest, "URL");
  cJSON *jsonFPS = cJSON_GetObjectItem(jsonRequest, "FPS");
  cJSON *jsonInputFPS = cJSON_GetObjectItem(jsonRequest, "InputFPS");
  cJSON *jsonBitrate = cJSON_GetObjectItem(jsonRequest, "Bitrate");

  std::string inputType, accel;
  int srcWidth = 1280;
  int srcHeight = 720;
  bool hevcEncode = false;
  int dstWidth = 640;
  int dstHeight = 480;
  int fps = 24;
  int inputfps = 0;
  int bitrate = 1000;

  if (jsonInputType && !cJSON_IsString(jsonInputType)) {
    tlog(TLOG_INFO, "wrong data, data no input type");
    return GetJsonByCode(RETCODE_TYPE_WRONG_DATA, "no input type");
  }

  if (cJSON_IsString(jsonInputType))
    inputType = jsonInputType->valuestring;

  if (cJSON_IsString(jsonAccel))
    accel = jsonAccel->valuestring;

  if (!cJSON_IsString(jsonDevice)) {
    tlog(TLOG_INFO, "wrong data, data no device");
    return GetJsonByCode(RETCODE_TYPE_WRONG_DATA, "no  device");
  }
  if (!cJSON_IsString(jsonURL)) {
    tlog(TLOG_INFO, "wrong data, data no URL");
    return GetJsonByCode(RETCODE_TYPE_WRONG_DATA, "no URL");
  }
  if (cJSON_IsNumber(jsonSrcWidth)) {
    if (jsonSrcWidth->valueint > 0)
      srcWidth = jsonSrcWidth->valueint;
  }
  if (cJSON_IsNumber(jsonSrcHeight)) {
    if (jsonSrcHeight->valueint > 0)
      srcHeight = jsonSrcHeight->valueint;
  }
  if (cJSON_IsTrue(jsonStreamCopy)) {
    hevcEncode = true;
  }
  if (cJSON_IsNumber(jsonDstWidth)) {
    if (jsonDstWidth->valueint > 0)
      dstWidth = jsonDstWidth->valueint;
  }
  if (cJSON_IsNumber(jsonDstHeight)) {
    if (jsonDstHeight->valueint > 0)
      dstHeight = jsonDstHeight->valueint;
  }
  if (cJSON_IsNumber(jsonFPS)) {
    if (jsonFPS->valueint > 0)
      fps = jsonFPS->valueint;
  }
  if (cJSON_IsNumber(jsonInputFPS)) {
    if (jsonInputFPS->valueint > 0)
      inputfps = jsonInputFPS->valueint;
  }
  if (cJSON_IsNumber(jsonBitrate)) {
    if (jsonBitrate->valueint > 0)
      bitrate = jsonBitrate->valueint;
  }

  tlog(TLOG_INFO,
       "start stream, inputType(%s), device(%s), accel(%s), srcWidth(%d), "
       "srcHeight(%d), hevcEncode(%d), "
       "dstWidth(%d), dstHeight(%d), fps(%d), inputfps(%d), bitrate(%d), URL(%s)",
       inputType.c_str(), jsonDevice->valuestring, accel.c_str(), srcWidth,
       srcHeight, hevcEncode, dstWidth, dstHeight, fps, inputfps, bitrate,
       jsonURL->valuestring);
  int ret = handler.StartStream(
      inputType.c_str(), (const char *)jsonDevice->valuestring, accel.c_str(),
      srcWidth, srcHeight, hevcEncode, dstWidth, dstHeight, fps, inputfps, bitrate,
      (const char *)jsonURL->valuestring);
  if (ret != 0) {
    tlog(TLOG_ERROR, "start stream failed. ret=%d", ret);
    return GetJsonByCode(RETCODE_TYPE_DEVICE_NOTOPEN, "device not open");
  }
  return GetJsonByCode(RETCODE_TYPE_SUCCESS, "success");
}

cJSON *UnixDomainService::CloseStream_Parse(cJSON *jsonRequest) {
  tlog(TLOG_INFO, "%s, close stream:\"%s\"", __FUNCTION__,
       cJSON_Print(jsonRequest));

  cJSON *jsonDevice = cJSON_GetObjectItem(jsonRequest, "Device");
  if (!cJSON_IsString(jsonDevice)) {
    tlog(TLOG_INFO, "wrong data, data no device");
    return GetJsonByCode(RETCODE_TYPE_WRONG_DATA, "no  device");
  }
  int ret = handler.StopStream((const char *)jsonDevice->valuestring);
  if (ret != 0) {
    tlog(TLOG_INFO, "stop stream failed, device(%s)", jsonDevice->valuestring);
    return GetJsonByCode(RETCODE_TYPE_UNKNOWN_ERROR, "unknown error");
  }
  return GetJsonByCode(RETCODE_TYPE_SUCCESS, "success");
}

cJSON *UnixDomainService::UnPacket(unsigned char *buf, int size) {
  if (size < MESSAGE_HEADER_SIZE) {
    tlog(TLOG_INFO, "wrong data, data size is %d", size);
    return NULL;
  }
  if (buf[0] != 0x20 || buf[1] != 0x21 || buf[2] != 0x09 || buf[3] != 0x01) {
    tlog(TLOG_INFO, "wrong data, data header is %02x %02x %02x %02x", buf[0],
         buf[1], buf[2], buf[3]);
    return NULL;
  }
  int dataSize = htonl(*(int *)(buf + 4));
  if (dataSize + MESSAGE_HEADER_SIZE != size) {
    tlog(TLOG_INFO, "wrong data, expected data size %d, actual data size %d",
         dataSize + MESSAGE_HEADER_SIZE, size);
    return NULL;
  }

  cJSON *json = cJSON_ParseWithLength((const char *)(buf + MESSAGE_HEADER_SIZE),
                                      dataSize);
  if (!cJSON_IsObject(json)) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
      tlog(TLOG_INFO, "[%s]...", error_ptr);
    }
    buf[50] = 0;
    tlog(TLOG_INFO, "wrong data, data format error,\"%s\"", (char *)(buf + 8));
    cJSON_Delete(json);
    return NULL;
  }
  return json;
}

cJSON *UnixDomainService::GetJsonByCode(int retCode, const char *msg) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddNumberToObject(json, "RetCode", retCode);
  cJSON_AddStringToObject(json, "Message", msg);
  return json;
}

int UnixDomainService::Packet(unsigned char *buf, int bufSize,
                              cJSON *jsonData) {
  char *stringData = cJSON_Print(jsonData);
  int stringSize = strlen(stringData);
  if (stringSize > bufSize - MESSAGE_HEADER_SIZE)
    stringSize = bufSize - MESSAGE_HEADER_SIZE;
  buf[0] = 0x01;
  buf[1] = 0x09;
  buf[2] = 0x21;
  buf[3] = 0x20;
  *(int *)(buf + 4) = ntohl(stringSize);
  memcpy(buf + MESSAGE_HEADER_SIZE, stringData, stringSize);
  cJSON_free(stringData);
  return stringSize + MESSAGE_HEADER_SIZE;
}

int UnixDomainService::PacketErrorMsg(unsigned char *buf, int bufSize,
                                      int retCode, const char *msg) {
  snprintf((char *)(buf + MESSAGE_HEADER_SIZE), bufSize - MESSAGE_HEADER_SIZE,
           "{\"RetCode\":%d,\"Message\":\"%s\"}", retCode, msg);
  int stringSize = strlen((char *)(buf + MESSAGE_HEADER_SIZE));
  if (stringSize > bufSize - MESSAGE_HEADER_SIZE)
    stringSize = bufSize - MESSAGE_HEADER_SIZE;
  buf[0] = 0x01;
  buf[1] = 0x09;
  buf[2] = 0x21;
  buf[3] = 0x20;
  *(int *)(buf + 4) = ntohl(stringSize);
  return stringSize + MESSAGE_HEADER_SIZE;
}
