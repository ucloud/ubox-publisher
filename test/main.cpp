#include "control_service.grpc.pb.h"
#include <grpc++/channel.h>
#include <grpc++/create_channel.h>
#include <unistd.h>

using namespace republish;

class ControlClient {
public:
    ControlClient(std::shared_ptr<grpc::Channel> channel)
    : stub_(ControlService::NewStub(channel)) {

    }

    void SetBitrate(int bitrate) {
        SetBitrateRequest request;
        SetBitrateResponse response;
        grpc::ClientContext context;

        request.set_bitrate(bitrate);
        grpc::Status status = stub_->SetBitrate(&context, request, &response);
        if (!status.ok()) {
            std::cout << "setbitrate rpc failed." << std::endl;
            return;
        } else {
            std::cout << response.result() << std::endl;
            return;
        }
    }

    void GetBitrate(TimeInterval interval) {
        GetBitrateRequest request;
        GetBitrateResponse response;
        grpc::ClientContext context;

        request.set_interval(interval);
        grpc::Status status = stub_->GetBitrate(&context, request, &response);
        if (!status.ok()) {
            std::cout << "getbitrate rpc failed." << std::endl;
            return;
        } else {
            std::cout << "interval " << response.interval()  << " bitrate " << response.bitrate() <<  std::endl;
            return;
        }
    }

private:
    std::unique_ptr<ControlService::Stub> stub_;
};

int main(int argc, char** argv) {
    ControlClient client( grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials()) );
    int type = atoi(argv[2]);
    if (type == 1) {
        client.SetBitrate(atoi(argv[3]));
    } else if (type == 2){
        client.GetBitrate(SECOND);
        client.GetBitrate(THREE_SECOND);
        client.GetBitrate(FIVE_SECOND);
        client.GetBitrate(TEN_SECOND);
        client.GetBitrate(THIRTY_SECOND);
        client.GetBitrate(MINUTE);
    } else if (type == 3) {
        srand((unsigned)time(NULL));
        while (true) {
            int t = rand()%6;
            int bitrate = 0;
            switch(t) {
                case 0:
                    bitrate = 10;
                    break;
                case 1:
                    bitrate = 200;
                    break;
                case 2:
                    bitrate = 500;
                    break;
                case 3:
                    bitrate = 800;
                    break;
                case 4:
                    bitrate = 1000;
                    break;
                case 5:
                    bitrate = 2000;
                    break;
                default:
                    bitrate = 100;
            }
            client.SetBitrate(bitrate);
            usleep(10000);
        }
    }
    return 0;
}

