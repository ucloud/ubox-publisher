rtmp stream publisher
=================
### Feature

1. hardware acceleration, support Intel HD graphics/Jetson
2. RTSP/v4l2/custom FPGA pull, rtmp push

### Performance

| hardware platform | input | output | cpu usage |
| --- | --- | --- | --- |
| Jetson Xavier NX | v4l2,1280*720,YUYV,30fps | rtmp,h264 high yuv420p,640*360,30fps,2Mbps |  12% |
| Intel J1900 | rtsp,h264 main yuv420p,1280*720,25fps |  rtmp,h264 high yuv420p,640*480,25fps,2Mbps | 17% |

### Install
1. dependency

* CentOS 8 (gstreamer-vaapi self compiling)
```bash
# setup rpmfusion repo
yum install gstreamer1 gstreamer1-libav gstreamer1-plugins-base.x86_64 gstreamer1-plugins-bad-free.x86_64 \
    gstreamer1-plugins-good.x86_64 gstreamer1-plugins-ugly.x86_64 gstreamer1-plugins-ugly-free.x86_64 \
    libva-intel-driver.x86_64 gstreamer1-plugins-bad-freeworld
```

* Ubuntu 18.04
```bash
apt-get install gstreamer1.0-plugins-ugly gstreamer1.0-plugins-good gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-bad gstreamer1.0-tools gstreamer1.0-rtsp gstreamer1.0-vaapi  gstreamer1.0-libav
```

2. compile

```bash
mkdir build && cd build && cmake .. && make install
```

### Test && Usage

1. run server,
```bash
ubox-publisher -u @/tmp/publisher.sock
```

2. test
```bash
# adjust json file
./build/test/cli @/tmp/publisher.sock < ./test/request/PushStream.json
```
