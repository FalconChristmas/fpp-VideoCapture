#include <fpp-pch.h>

#include "VideoCaptureEffect.h"

#include <sys/ioctl.h>
#include <sys/mman.h>

#include <libcamera/camera_manager.h>
#include <libcamera/camera.h>
#include <libcamera/property_ids.h>
#include <libcamera/formats.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
using namespace libcamera;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

static AVPixelFormat mapPixelFormat(PixelFormat &fm) {
    switch (fm) {
    case formats::RGB565: return AV_PIX_FMT_RGB565LE;
    case formats::BGR888: return AV_PIX_FMT_BGR24;
    case formats::RGB888: return AV_PIX_FMT_RGB24;
    case formats::XRGB8888: return AV_PIX_FMT_0RGB;
    case formats::XBGR8888: return AV_PIX_FMT_0BGR;
    case formats::BGRX8888: return AV_PIX_FMT_BGR0;
    case formats::ABGR8888: return AV_PIX_FMT_ABGR;
    case formats::ARGB8888: return AV_PIX_FMT_ARGB;
    case formats::BGRA8888: return AV_PIX_FMT_BGRA;
    case formats::RGBA8888: return AV_PIX_FMT_RGBA;
    case formats::YUYV: return AV_PIX_FMT_YUYV422;
    case formats::YVYU: return AV_PIX_FMT_YVYU422;
    case formats::UYVY: return AV_PIX_FMT_UYVY422;
    case formats::VYUY: return AV_PIX_FMT_YVYU422;
    case formats::NV12: return AV_PIX_FMT_NV12;
    case formats::NV21: return AV_PIX_FMT_NV21;
    case formats::NV16: return AV_PIX_FMT_NV16;
    case formats::NV24: return AV_PIX_FMT_NV24;
    case formats::NV42: return AV_PIX_FMT_NV42;
    case formats::YUV420: return AV_PIX_FMT_YUV420P;
    case formats::YUV422: return AV_PIX_FMT_YUYV422;

    default:
        break;
    }
    return AV_PIX_FMT_NONE;
}


class LibCameraVideoCaptureEffect : public VideoCaptureEffect {
public:
    LibCameraVideoCaptureEffect() : VideoCaptureEffect() {
        cameras = new CameraManager();
        cameras->start();

        if (!cameras->cameras().empty()) {
            PixelOverlayEffect::AddPixelOverlayEffect(this);
        }
    }
    virtual ~LibCameraVideoCaptureEffect() {
        cameras->stop();
        delete cameras;
        PixelOverlayEffect::RemovePixelOverlayEffect(this);
    }
    virtual void ListCameras(Json::Value &camerasJson) {
        for (auto &a : cameras->cameras()) {
            camerasJson[a->id()] = a->properties().get(libcamera::properties::Model);
        }
    }


    virtual bool apply(PixelOverlayModel* model, const std::string& ae, const std::vector<std::string>& args) {
        if (args.size() < 1) {
            return false;
        }
        std::string cid = args[0];
        if (cid == "--Default--") {
            cid = cameras->cameras().front()->id();
        }
        auto camera = cameras->get(cid);
        camera->acquire();
        VCRunningEffect* re = new VCRunningEffect(camera, model, ae, args);
        model->setRunningEffect(re, 1);
        return true;
    }

    class VCRunningEffect : public RunningEffect {
    public:
        VCRunningEffect(std::shared_ptr<Camera> c, PixelOverlayModel* m, const std::string& ae, const std::vector<std::string>& args) :
            RunningEffect(m),
            camera(c),
            autoEnable(false) {

            PixelOverlayState st(ae);
            if (st.getState() != PixelOverlayState::PixelState::Disabled) {
                autoEnable = true;
                model->setState(st);
            }
            auto config = camera->generateConfiguration( { StreamRole::VideoRecording});
            StreamConfiguration &sc = config->at(0);
            sc.size.width = m->getWidth();
            sc.size.height = m->getHeight();
            sc.pixelFormat = libcamera::formats::RGB888;
            sc.bufferCount = 6;
            config->validate();
            if (sc.size.width < m->getWidth() || sc.size.height < m->getHeight()) {
                //we'd rather scale down than up so try a bit larger and
                //see if we can get a better resolution image
                sc.size.width = m->getWidth() * 3 / 2;
                sc.size.height = m->getHeight() * 2 / 2;
                sc.pixelFormat = libcamera::formats::RGB888;
                config->validate();
            }
            // couldn't get RGB888, see if we can get one of the formats that we can convert
            // relatively easily
            if (sc.pixelFormat != libcamera::formats::RGB888) {
                sc.pixelFormat = formats::YUYV;
                config->validate();
                if (sc.pixelFormat != libcamera::formats::YUYV) {
                    sc.pixelFormat = formats::YVYU;
                    config->validate();
                    if (sc.pixelFormat != libcamera::formats::YVYU) {
                        sc.pixelFormat = formats::YUV420;
                        config->validate();
                        if (sc.pixelFormat != libcamera::formats::YUV420) {
                            sc.pixelFormat = formats::YUV422;
                            config->validate();
                        }
                    }
                }
            }
            camera->configure(config.get());

            allocator = new FrameBufferAllocator(camera);
            allocator->allocate(sc.stream());
            camera->start();

            if (sc.size.width != m->getWidth()
                || sc.size.height != m->getHeight()
                || sc.pixelFormat != libcamera::formats::RGB888) {

                AVPixelFormat pf = mapPixelFormat(sc.pixelFormat);
                if (pf == AV_PIX_FMT_NONE) {
                    printf("WARN WARN WARN WARN\n");
                }
                swsCtx = sws_getContext(sc.size.width, sc.size.height, pf, 
                                        m->getWidth(), m->getHeight(), AVPixelFormat::AV_PIX_FMT_RGB24,
                                        SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

                srcFrame = av_frame_alloc();
                srcFrame->width = sc.size.width;
                srcFrame->height = sc.size.height;
                srcFrame->format = pf;

                av_frame_get_buffer(srcFrame, 0);
                dstFrame = av_frame_alloc();
                dstFrame->width =  m->getWidth();
                dstFrame->height = m->getHeight();
                dstFrame->format = AVPixelFormat::AV_PIX_FMT_RGB24;
                dstFrame->linesize[0] = m->getWidth() * 3;
                av_frame_get_buffer(dstFrame, 0);
            }

            numBuffers = sc.bufferCount;
            if (numBuffers > 6) {
                numBuffers = 6;
            }
            for (int x = 0; x < numBuffers; x++) {
                requests[x] = camera->createRequest();
                requests[x]->reuse(Request::ReuseFlag::ReuseBuffers);
                requests[x]->addBuffer(sc.stream(), allocator->buffers(sc.stream())[x].get());
                bufferSize = allocator->buffers(sc.stream())[x]->planes()[0].length;

                buffers[x] = (uint8_t*)mmap(NULL, bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED,  allocator->buffers(sc.stream())[x]->planes()[0].fd.get(), 0);
                camera->queueRequest(requests[x].get());
            }
        }
        virtual ~VCRunningEffect() {
            camera->stop();
            for (int x = 0; x < numBuffers; x++) {
                if (buffers[x]) {
                    munmap(buffers[x], bufferSize);
                }
            }
            if (swsCtx) {
                av_frame_free(&srcFrame);
                av_frame_free(&dstFrame);
                sws_freeContext(swsCtx);
            }

            camera->release();
            delete allocator;
        }
        const std::string& name() const override {
            static std::string NAME = "Video Capture";
            return NAME;
        }
        virtual int32_t update() override {
            //printf("Status: %d     Buffers:  %d    Seq:  %d\n", requests[curRequest]->status(), requests[curRequest]->hasPendingBuffers(), requests[curRequest]->sequence());
            //printf("      : %d     Buffers:  %d    Seq:  %d\n", requests[curRequest+1]->status(), requests[curRequest+1]->hasPendingBuffers(), requests[curRequest+1]->sequence());
            if (requests[curRequest]->status() == Request::Status::RequestComplete) {
                if (swsCtx) {
                    uint8_t *olds = srcFrame->data[0];
                    uint8_t *oldd = dstFrame->data[0];
                    srcFrame->data[0] = buffers[curRequest];
                    dstFrame->data[0] = model->getOverlayBuffer();

                    sws_scale(swsCtx, 
                                srcFrame->data, srcFrame->linesize,
                                0, srcFrame->height, 
                                dstFrame->data, dstFrame->linesize);

                    srcFrame->data[0] = olds;
                    dstFrame->data[0] = oldd;
                } else {
                    memcpy(model->getOverlayBuffer(), buffers[curRequest], model->getWidth() * model->getHeight() * 3);
                }
                model->setOverlayBufferDirty(true);

                requests[curRequest]->reuse(Request::ReuseFlag::ReuseBuffers);
                camera->queueRequest(requests[curRequest].get());
                curRequest++;
                if (curRequest == numBuffers) {
                    curRequest = 0;
                }
            }
            return 25;
        }
        std::shared_ptr<Camera> camera;
        FrameBufferAllocator *allocator;
        bool autoEnable;

        std::unique_ptr<Request> requests[6];
        uint8_t *buffers[6];
        int curRequest = 0;
        int numBuffers = 6;

        uint8_t *buffer = nullptr;
        size_t bufferSize = 0;
        SwsContext *swsCtx = nullptr;
        AVFrame *srcFrame = nullptr;
        AVFrame *dstFrame = nullptr;
    };

    libcamera::CameraManager *cameras = nullptr;
};




VideoCaptureEffect *VideoCaptureEffect::createVideoCaptureEffect() {
    return new LibCameraVideoCaptureEffect();
}
