#!/usr/bin/env bash

cd `dirname ${BASH_SOURCE[@]}`

BUILDDIR=../../build
PUBLISHER_CMD=$BUILDDIR/bin/ubox-publisher
CLI_CMD=$BUILDDIR/test/cli


start_publisher() {
    nohup sudo $PUBLISHER_CMD -u @/tmp/publisher.sock > publisher.log 2>&1 &
    echo "$!" > .publisher.pid
}

stop_publisher() {
    local pid=$(cat .publisher.pid)
    ps -ef | grep -i ubox-publisher | grep -i $pid | awk '{print $2}' | xargs sudo kill
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
    timeout 15 ffprobe $RTMPURL 2>&1 | grep -iq $s
    if [[ $? -eq 0 ]]; then
        echo "test pass"
    else
        echo "test fail"
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
    echo "do test $device $encode $decode $tmpurl $accel, expect $result"
    make_push_stream_request "$device" "$encode" "$decode" "$rtmpurl" "$accel"
    $CLI_CMD @/tmp/publisher.sock < .tmp.json
    check_result "$result"
    echo "{\"Action\":\"CloseStream\",\"Device\":\"/dev/video0\"}" | $CLI_CMD @/tmp/publisher.sock 
    sleep 3
}

. test.conf

start_publisher
sleep 2

#v4l2
do_test $DEVICE h264 h264 $RTMPURL intel h264
do_test $DEVICE h264 h265 $RTMPURL intel h265
do_test $DEVICE h264 h264 $RTMPURL none h264
do_test $DEVICE h264 h265 $RTMPURL none h265

#rtsp
do_test $DEVICE_RTSP_H264 h264 h264 $RTMPURL intel h264
do_test $DEVICE_RTSP_H265 h265 h264 $RTMPURL intel h264
do_test $DEVICE_RTSP_H264 h264 h264 $RTMPURL none h264
do_test $DEVICE_RTSP_H265 h265 h264 $RTMPURL none h264

stop_publisher
