#!/usr/bin/env bash
#run as root
#depend pidstat, ffmpeg(h265 in rtmp)

cd `dirname ${BASH_SOURCE[@]}`

. test.conf

PUBLISHER_CMD=$BUILDDIR/bin/ubox-publisher
CLI_CMD=$BUILDDIR/test/cli
FFMPEG_CMD=${FFMPEG_CMD:-ffmpeg}
pid=

function log() {
    echo "`date +%Y.%m.%d-%H:%M:%S.%N` [$$] $*"
}

start_publisher() {
    log "start ubox-publisher in background"
    nohup $PUBLISHER_CMD -u @/tmp/publisher.sock > publisher.log 2>&1 &
    pid=$!
}

stop_publisher() {
    log "stop ubox-publisher"
    ps -ef | grep -i ubox-publisher | grep -i $pid | awk '{print $2}' | xargs kill
}

make_push_stream_request() {
    local device=$1
    local decode=$2
    local encode=$3
    local rtmpurl=$4
    local accel=$5
    echo "{\"Action\":\"PushStream\", \"Encode\":\"$encode\",\"Decode\":\"$decode\",\"Accel\":\"$accel\",\"Device\": \"$device\", \"SrcWidth\":1280, \"SrcHeight\":720, \"URL\":\"$rtmpurl\", \"FPS\":30,\"Bitrate\":800, \"ClockEnable\":true}" > .tmp.json
}

check_result() {
    local s=$1
    timeout 15 $FFMPEG_CMD -nostdin -hide_banner -i $rtmpurl 2>&1 | tee .ffmpeg.log | grep -iq "Video: $s"
    if [[ $? -eq 0 ]]; then
        log "test pass, cpu $(pidstat -p $pid 1 2 | grep -i ave | awk '{print $8}')%"
        $FFMPEG_CMD -nostdin -hide_banner -i $rtmpurl -frames:v 1 $s-$(date +%H_%M_%S).jpeg
    else
        log "test fail"
        stop_publisher
        exit 1
    fi
}

do_test() {
    local device=$1
    local decode=$2
    local encode=$3
    local rtmpurl=$4
    local accel=$5
    local result=$6
    log "do test $device,decoder=$decode,encoder=$encode,$rtmpurl,accel=${accel:-auto}, expect $result"
    make_push_stream_request "$device" "$decode" "$encode" "$rtmpurl" "$accel"
    $CLI_CMD @/tmp/publisher.sock < .tmp.json
    if [[ $encode == "h265" && ($platform == "jetson" || $accel == "jetson") ]]; then
        log "skip check result"
        sleep 10
    else
        check_result "$result"
    fi
    echo "{\"Action\":\"CloseStream\",\"Device\":\"/dev/video0\"}" | $CLI_CMD @/tmp/publisher.sock 
    sleep 10
}

test_platform() {
    #v4l2
    log "test v4l2 begin"
    log "test h264"
    do_test $DEVICE "" h264 $RTMPURL "" h264
    do_test $DEVICE "" h264 $RTMPURL none h264

    log "test h265"
    if [[ $platform != "j1900" ]]; then
        do_test $DEVICE "" h265 $RTMPURL "" hevc
    fi
    do_test $DEVICE "" h265 $RTMPURL none hevc
    log "test v4l2 end"

    #rtsp
    log "test rtsp begin"
    do_test $DEVICE_RTSP_H264 h264 h264 $RTMPURL "" h264
    if [[ $platform != "j1900" ]]; then
        do_test $DEVICE_RTSP_H265 h265 h264 $RTMPURL "" h264
    fi
    do_test $DEVICE_RTSP_H264 h264 h264 $RTMPURL none h264
    do_test $DEVICE_RTSP_H265 h265 h264 $RTMPURL none h264
    log "test rtsp end"
}

usage() {
    echo "usage:$0 [j1900|jetson|all]"
    echo "usage:$0 device decoder encoder rtmp_url accel check_string"
}

if [[ $# -eq 0 || $1 == "-h" || $1 == "--help" ]]; then
    usage
    exit 1
fi

if [[ $(id -u) -ne 0 ]]; then
    echo "run as root"
    exit 1
fi

start_publisher
sleep 2

if [[ $# -eq 6 ]]; then
    do_test $1 $2 $3 $4 $5 $6
elif [[ $# -eq 1 ]]; then
    platform=$1
    test_platform $platform
else
    usage
fi

stop_publisher
