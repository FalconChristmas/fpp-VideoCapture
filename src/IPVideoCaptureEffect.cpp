#include <fpp-pch.h>

#include "VideoCaptureEffect.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}



class IPVCRunningEffect : public RunningEffect {
    public:
    IPVCRunningEffect(PixelOverlayModel* m, const std::string& ae, const std::vector<std::string>& args,
        AVFormatContext* fc, const AVCodec *d, int idx) :
        RunningEffect(m),
        autoEnable(false),
        format_ctx(fc), 
        decoder(d),
        video_stream_index(idx) {

        PixelOverlayState st(ae);
        if (st.getState() != PixelOverlayState::PixelState::Disabled) {
            autoEnable = true;
            model->setState(st);
        }
        captureThread = new std::thread([this] { captureLoop(); });
    }

    virtual ~IPVCRunningEffect() {
        running = false;
        captureThread->join();
        avformat_free_context(format_ctx);
    }
    const std::string& name() const override {
        static std::string NAME = "IP Video Capture";
        return NAME;
    }
    virtual int32_t update() override {
        if (dstFrame) {
            memcpy(model->getOverlayBuffer(), dstFrame->data[0], model->getWidth() * model->getHeight() * 3);
            model->setOverlayBufferDirty(true);
        }
        return 25;
    }


    void captureLoop() {
        AVPacket packet;
        AVCodecContext *ccontext = avcodec_alloc_context3(decoder);
        avcodec_open2(ccontext, NULL, NULL);
        AVFrame* pic = av_frame_alloc();
        SwsContext *swsCtx = nullptr;
        while (running) {
            while (av_read_frame(format_ctx, &packet) >=0 && running) {
                if (packet.stream_index == video_stream_index) {
                    int ret = avcodec_send_packet(ccontext, &packet);
                    while (ret != 0) {
                        int rc = avcodec_receive_frame(ccontext, pic);
                        if (rc == 0) {
                            if (swsCtx == nullptr) {
                                int w = model->getWidth();
                                int h = model->getHeight();
                                swsCtx = sws_getContext(pic->width, pic->height, (AVPixelFormat)pic->format, 
                                                        w, h, AVPixelFormat::AV_PIX_FMT_RGB24,
                                                        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

                                dstFrame = av_frame_alloc();
                                dstFrame->width =  w;
                                dstFrame->height = h;
                                dstFrame->format = AVPixelFormat::AV_PIX_FMT_RGB24;
                                dstFrame->linesize[0] = w * 3;
                                av_frame_get_buffer(dstFrame, 0);
                            }
                            sws_scale(swsCtx, pic->data, pic->linesize, 0, pic->height, 
                                      dstFrame->data, dstFrame->linesize);
                        }
                        ret = avcodec_send_packet(ccontext, &packet);
                    }
                }
                av_packet_unref(&packet);
            }
        }

        av_frame_free(&pic);
        avcodec_close(ccontext);
        if (swsCtx) {
            av_frame_free(&dstFrame);
            dstFrame = nullptr;
            sws_freeContext(swsCtx);
        }
    }

    AVFormatContext* format_ctx = nullptr;
    const AVCodec* decoder = nullptr;
    AVFrame* dstFrame = nullptr;
    int video_stream_index = -1;
    bool autoEnable;
    volatile bool running = true;
    std::thread *captureThread = nullptr;
};


IPVideoCaptureEffect::IPVideoCaptureEffect() : PixelOverlayEffect("IP Video Capture") {
    args.push_back(CommandArg("URL", "string", "URL", false));
    //args.push_back(CommandArg("UserName", "string", "User Name", true));
    //args.push_back(CommandArg("Password", "password", "Password", true));

    avformat_network_init();

    PixelOverlayEffect::AddPixelOverlayEffect(this);
}

IPVideoCaptureEffect::~IPVideoCaptureEffect() {
    PixelOverlayEffect::RemovePixelOverlayEffect(this);
}

bool IPVideoCaptureEffect::apply(PixelOverlayModel* model, const std::string& ae, const std::vector<std::string>& args) {

    std::string url = args[0];
    std::string username = args.size() > 1 ? args[1] : "";
    std::string password = args.size() > 2 ? args[2] : "";

    if (username != "") {
        std::string nurl = url.substr(0, url.find("//") + 2);
        nurl += username;
        nurl += ":";
        nurl += password;
        nurl += url.substr(url.find("//") + 2);
        url = nurl;
    }
    printf("connecting to %s\n", url.c_str());

    AVFormatContext* format_ctx = avformat_alloc_context();
    const AVCodec* decoder = nullptr;
    int video_stream_index = -1;

    if (avformat_open_input(&format_ctx, url.c_str(), NULL, NULL) != 0) {
        printf("Could not connect\n");
        avformat_free_context(format_ctx);
        return false;
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        printf("Could not find stream\n");
        avformat_free_context(format_ctx);
        return false;
    }
    //search video stream
    video_stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (video_stream_index == -1) {
        printf("Could not find video stream\n");
        avformat_free_context(format_ctx);
        return false;
    }
    av_read_play(format_ctx);

    IPVCRunningEffect* re = new IPVCRunningEffect(model, ae, args, format_ctx, decoder, video_stream_index);
    model->setRunningEffect(re, 1);
    return true;
}
