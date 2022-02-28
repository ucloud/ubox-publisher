#!/usr/bin/env bash
#run as root

cd `dirname ${BASH_SOURCE[@]}`

BUILDDIR=../../build
PUBLISHER_CMD=$BUILDDIR/bin/ubox-publisher
CLI_CMD=$BUILDDIR/test/cli
pid=

function log() {
    echo "`date +%Y.%m.%d-%H:%M:%S.%N` [$$] $*"
}

start_publisher() {
    nohup $PUBLISHER_CMD -u @/tmp/publisher.sock > publisher.log 2>&1 &
    pid=$!
}

stop_publisher() {
    ps -ef | grep -i ubox-publisher | grep -i $pid | awk '{print $2}' | xargs kill
}

make_push_stream_request() {
    local device=$1
    local decode=$2
    local encode=$3
    local rtmpurl=$4
    local accel=$5
    echo "{\"Action\":\"PushStream\", \"Encode\":\"$encode\",\"Decode\":\"$decode\",\"Accel\":\"$accel\",\"Device\": \"$device\", \"SrcWidth\":1280, \"SrcHeight\":720, \"URL\":\"$rtmpurl\", \"FPS\":30,\"Bitrate\":800}" > .tmp.json
}

check_result() {
    local s=$1
    timeout 15 ffprobe -hide_banner $RTMPURL 2>&1 | grep -iq "Video: $s"
    if [[ $? -eq 0 ]]; then
        log "test pass, cpu $(pidstat -p $pid 1 2 | grep -i ave | awk '{print $8}')%"
    else
        log "test fail"
        exit 1
    fi
}

do_test() {
    local device=$1
    local encode=$2
    local decode=$3
    local rtmpurl=$4
    local accel=$5
    local result=$6
    log "do test $device,$encode,$decode,$rtmpurl,$accel, expect $result"
    make_push_stream_request "$device" "$encode" "$decode" "$rtmpurl" "$accel"
    $CLI_CMD @/tmp/publisher.sock < .tmp.json
    check_result "$result"
    echo "{\"Action\":\"CloseStream\",\"Device\":\"/dev/video0\"}" | $CLI_CMD @/tmp/publisher.sock 
    sleep 3
}

test_platform() {
    #v4l2
    do_test $DEVICE "" h264 $RTMPURL "" h264
    if [[ $platform != "j1900" && $platform != "jetson" ]]; then
        do_test $DEVICE "" h265 $RTMPURL "" hevc
    fi
    do_test $DEVICE "" h264 $RTMPURL none h264
    do_test $DEVICE "" h265 $RTMPURL none hevc
    
    #rtsp
    do_test $DEVICE_RTSP_H264 h264 h264 $RTMPURL "" h264
    if [[ $platform != "j1900" ]]; then
        do_test $DEVICE_RTSP_H265 h265 h264 $RTMPURL "" h264
    fi
    do_test $DEVICE_RTSP_H264 h264 h264 $RTMPURL none h264
    do_test $DEVICE_RTSP_H265 h265 h264 $RTMPURL none h264
}

if [[ $1 == "-h" || $1 == "--help" ]]; then
    echo "usage:$0 [j1900|jetson|all]"
    echo "usage:$0 device decoder encoder rtmp_url accel check_string"
    exit 1
fi

. test.conf

start_publisher
sleep 2

if [[ $# -eq 6 ]]; then
    do_test $1 $2 $3 $4 $5 $6
elif [[ $# -eq 1 ]]; then
    platform=$1
    test_platform $platform
else
    test_platform all
fi

stop_publisher
