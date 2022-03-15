#include <fpp-pch.h>

#include "VideoCaptureEffect.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/mman.h>


#include <linux/types.h>
#include <linux/v4l2-common.h>
#include <linux/v4l2-controls.h>
#include <linux/videodev2.h>
#include <linux/media.h>
#include <sys/ioctl.h>
#include <libv4l2.h>

class V4LVideoCaptureEffect : public VideoCaptureEffect {
public:
    V4LVideoCaptureEffect() : VideoCaptureEffect() {
        PixelOverlayEffect::AddPixelOverlayEffect(this);
    }
    virtual ~V4LVideoCaptureEffect() {
        PixelOverlayEffect::RemovePixelOverlayEffect(this);
    }
    virtual void ListCameras(Json::Value &camerasJson) {
        const std::string dev_folder = "/dev/";
        DIR *dir;
        struct dirent *ent;
        if ((dir = opendir(dev_folder.c_str())) != NULL) {
            while ((ent = readdir(dir)) != NULL) {
                std::string nm = ent->d_name;
                if (nm.size() > 5 && nm.substr(0, 5) == "video") {
                    std::string file = dev_folder + nm;
                    const int fd = open(file.c_str(), O_RDWR);
                    v4l2_capability capability;
                    if (fd >= 0 && ioctl(fd, VIDIOC_QUERYCAP, &capability) >= 0) {

                        if ((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE)
                            && (capability.capabilities & V4L2_CAP_STREAMING)
                            && (capability.capabilities & V4L2_CAP_META_CAPTURE)
                            && !(capability.device_caps & V4L2_CAP_META_CAPTURE)) {

                            //printf("%s   %X  %X\n", file.c_str(), capability.capabilities, capability.device_caps);
                            struct media_device_info mdi;
                            if (ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi) == 0) {
                                if (mdi.model[0]) {
                                    camerasJson[file] = std::string((const char *)mdi.model);
                                } else {
                                    camerasJson[file] = std::string((const char *)mdi.driver);
                                }
                            } else {
                                camerasJson[file] = std::string((const char *)capability.card);
                            }
                        }
                    }
                    close(fd);
                }
            }
            closedir(dir);
        }
    }


    virtual bool apply(PixelOverlayModel* model, const std::string& ae, const std::vector<std::string>& args) {
        if (args.size() < 1) {
            return false;
        }
        std::string cid = args[0];
        if (cid == "--Default--") {
            cid = "/dev/video0"; //FIXME - iterate and find first
        }
        if (!FileExists(cid)) {
            LogErr(VB_PLUGIN, "Video device %s does not exist\n",
                cid.c_str());
            return false;
        }

        int fd = v4l2_open(cid.c_str(), O_RDWR | O_NONBLOCK, 0);
        if (fd == -1) {
            LogErr(VB_PLUGIN, "Unable to open video device %s: %s\n",
                cid.c_str(), strerror(errno));
            return false;
        }


        VCRunningEffect* re = new VCRunningEffect(fd, model, ae, args);
        model->setRunningEffect(re, 1);
        return true;
    }

    class VCRunningEffect : public RunningEffect {
        struct v4l2Buffer {
            uint8_t *start;
            size_t  length;
        };
    public:
        VCRunningEffect(int fd, PixelOverlayModel* m, const std::string& ae, const std::vector<std::string>& args) :
            RunningEffect(m),
            m_fd(fd),
            autoEnable(false) {

            PixelOverlayState st(ae);
            if (st.getState() != PixelOverlayState::PixelState::Disabled) {
                autoEnable = true;
                model->setState(st);
            }
            
            struct v4l2_format              fmt;
            struct v4l2_buffer              buf;
            struct v4l2_requestbuffers      req;
            struct v4l2_streamparm          parm;

            memset(&fmt, 0, sizeof(fmt));
            memset(&parm, 0, sizeof(parm));
            memset(&req, 0, sizeof(req));
    
            fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            fmt.fmt.pix.width       = m->getWidth();
            fmt.fmt.pix.height      = m->getHeight();
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
            fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
            if (!IOCTLWrapper(m_fd, VIDIOC_S_FMT, &fmt)) {
                LogErr(VB_PLUGIN, "Error initializing format\n");
                return;
            }
            //printf("w: %d    h: %d     pf: %s\n", fmt.fmt.pix.width, fmt.fmt.pix.height, &fmt.fmt.pix.pixelformat);
            if (fmt.fmt.pix.width < m->getWidth() || fmt.fmt.pix.height < m->getHeight()) {
                //we'd rather scale down than up so try a bit larger and
                //see if we can get a better resolution image
                fmt.fmt.pix.width = m->getWidth() * 3 / 2;
                fmt.fmt.pix.height = m->getHeight() * 2 / 2;
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
                fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
                if (!IOCTLWrapper(m_fd, VIDIOC_S_FMT, &fmt)) {
                    LogErr(VB_PLUGIN, "Error initializing format\n");
                    return;
                }
            }
            bufferWidth = fmt.fmt.pix.width;
            bufferHeight = fmt.fmt.pix.height;
            //printf("w: %d    h: %d     pf: %s\n", fmt.fmt.pix.width, fmt.fmt.pix.height, &fmt.fmt.pix.pixelformat);

            parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            parm.parm.capture.timeperframe.numerator = 1;
            parm.parm.capture.timeperframe.denominator = 30;
            if (!IOCTLWrapper(m_fd, VIDIOC_S_PARM, &parm)) {
                LogErr(VB_PLUGIN, "Error initializing framerate\n");
                return;
            }
            req.count  = 2;
            req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            req.memory = V4L2_MEMORY_MMAP;
            if (!IOCTLWrapper(m_fd, VIDIOC_REQBUFS, &req)) {
                LogErr(VB_PLUGIN, "Error initializing request\n");
                return;
            }
            m_buffers = (v4l2Buffer*)calloc(req.count, sizeof(v4l2Buffer));
            for (m_bufferCount = 0; m_bufferCount < req.count; ++m_bufferCount) {
                memset(&buf, 0, sizeof(buf));
                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = m_bufferCount;

                IOCTLWrapper(m_fd, VIDIOC_QUERYBUF, &buf);

                m_buffers[m_bufferCount].length = buf.length;
                m_buffers[m_bufferCount].start = (uint8_t*)v4l2_mmap(NULL, buf.length,
                    PROT_READ | PROT_WRITE, MAP_SHARED,
                    m_fd, buf.m.offset);

                if (MAP_FAILED == m_buffers[m_bufferCount].start) {
                    LogErr(VB_PLUGIN, "Error mapping buffer: %s\n", strerror(errno));
                    return;
                }
            }
            for (int i = 0; i < m_bufferCount; ++i) {
                memset(&buf, 0, sizeof(buf));
                buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index  = i;
                IOCTLWrapper(m_fd, VIDIOC_QBUF, &buf);
            }

            enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (!IOCTLWrapper(m_fd, VIDIOC_STREAMON, &type)) {
                LogDebug(VB_PLUGIN, "VIDIOC_STREAMON call failed\n");
                return;
            }

        }
        virtual ~VCRunningEffect() {
            for (int i = 0; i < m_bufferCount; ++i) {
                v4l2_munmap(m_buffers[i].start, m_buffers[i].length);
            }

            if (m_fd != -1) {
                v4l2_close(m_fd);
            }
            free(m_buffers);
        }
        const std::string& name() const override {
            static std::string NAME = "Video Capture";
            return NAME;
        }
        virtual int32_t update() override {           
            struct v4l2_buffer  buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            if (v4l2_ioctl(m_fd, VIDIOC_DQBUF, &buf) == 0) {
                //frame is available
                model->setScaledData(m_buffers[buf.index].start, bufferWidth, bufferHeight);
                // re-queue the buffer
                if (!IOCTLWrapper(m_fd, VIDIOC_QBUF, &buf)) {
                    LogErr(VB_PLUGIN, "VIDIOC_QBUF call failed\n");
                }
            }

            return 25;
        }
        bool IOCTLWrapper(int fh, int request, void *arg) {
            int ret;
            if (m_fd == -1) {
                return false;
            }

            do {
                ret = v4l2_ioctl(fh, request, arg);
            } while (ret == -1 && ((errno == EINTR) || (errno == EAGAIN)));

            if (ret == -1) {
                LogErr(VB_PLUGIN, "v4l2_ioctl() failed: %s\n", strerror(errno));
                return false;
            }

            return true;
        }


        v4l2Buffer *m_buffers;
        int m_bufferCount = 0;
        int bufferWidth, bufferHeight;
        bool autoEnable;
        int m_fd = -1;
    };
};


VideoCaptureEffect *VideoCaptureEffect::createVideoCaptureEffect() {
    return new V4LVideoCaptureEffect();
}

