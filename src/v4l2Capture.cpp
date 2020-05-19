#include "v4l2Capture.h"

#include <sys/mman.h>

#include "log.h"

v4l2Capture::v4l2Capture(std::string deviceName, unsigned int width,
    unsigned int height, unsigned int fps)
  : m_fd(-1),
    m_deviceName(deviceName),
    m_width(width),
    m_height(height),
    m_fps(fps),
    m_captureThread(NULL),
    m_captureFrames(false),
    m_frame(NULL)
{
    LogDebug(VB_PLUGIN, "v4l2Capture::v4l2Capture(%s, %d, %d)\n",
        m_deviceName.c_str(), m_width, m_height);
}

v4l2Capture::~v4l2Capture() {
    Close();
}

bool v4l2Capture::Init()
{
    LogDebug(VB_PLUGIN, "v4l2Capture::Init()\n");

    struct v4l2_format              fmt;
    struct v4l2_buffer              buf;
    struct v4l2_requestbuffers      req;
    struct v4l2_streamparm          parm;

    m_fd = v4l2_open(m_deviceName.c_str(), O_RDWR | O_NONBLOCK, 0);
    if (m_fd < 0) {
        LogErr(VB_PLUGIN, "Unable to open video device %s: %s\n",
            m_deviceName.c_str(), strerror(errno));
        return false;
    }

    bzero(&fmt, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width       = m_width;
    fmt.fmt.pix.height      = m_height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
    fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

    if (!IOCTLWrapper(m_fd, VIDIOC_S_FMT, &fmt)) {
        LogErr(VB_PLUGIN, "Error initializing format\n");
        return false;
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
        LogErr(VB_PLUGIN, "Unable to set libv4l to RGB format\n");
        return false;
    }

    if ((fmt.fmt.pix.width != m_width) ||
        (fmt.fmt.pix.height != m_height)) {
        LogErr(VB_PLUGIN, "Unable to set libv4l to %dx%d dimensions\n",
            m_width, m_height);
    }

    bzero(&req, sizeof(req));

    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = m_fps;

    if (!IOCTLWrapper(m_fd, VIDIOC_S_PARM, &parm)) {
        LogErr(VB_PLUGIN, "Error initializing framerate\n");
        return false;
    }

    req.count  = 2;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (!IOCTLWrapper(m_fd, VIDIOC_REQBUFS, &req)) {
        LogErr(VB_PLUGIN, "Error initializing request\n");
        return false;
    }

    m_buffers = (v4l2Buffer *)calloc(req.count, sizeof(*m_buffers));
    for (m_bufferCount = 0; m_bufferCount < req.count; ++m_bufferCount) {
        bzero(&buf, sizeof(buf));

        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = m_bufferCount;

        IOCTLWrapper(m_fd, VIDIOC_QUERYBUF, &buf);

        m_buffers[m_bufferCount].length = buf.length;
        m_buffers[m_bufferCount].start = v4l2_mmap(NULL, buf.length,
              PROT_READ | PROT_WRITE, MAP_SHARED,
              m_fd, buf.m.offset);

        if (MAP_FAILED == m_buffers[m_bufferCount].start) {
            LogErr(VB_PLUGIN, "Error mapping buffer: %s\n", strerror(errno));
            return false;
        }
    }

    for (int i = 0; i < m_bufferCount; ++i) {
        bzero(&buf, sizeof(buf));
        buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        IOCTLWrapper(m_fd, VIDIOC_QBUF, &buf);
    }

    m_frame = (unsigned char *)malloc(m_width * m_height * 3);

    StartCapture();

    LogDebug(VB_PLUGIN, "v4l2Capture::Init() Success\n");

    return true;
}

void v4l2Capture::StartCapture()
{
    LogDebug(VB_PLUGIN, "v4l2Capture::StartCapture()\n");

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    IOCTLWrapper(m_fd, VIDIOC_STREAMON, &type);

    m_captureFrames = true;
    m_captureThread = new std::thread([this]() {
        fd_set             fds;
        struct timeval     tv;
        int                ret;
        struct v4l2_buffer buf;
        unsigned long long frameCount = 0;

        LogDebug(VB_PLUGIN, "v4l2Capture: Starting capture thread\n");

        while (m_captureFrames) {
            do {
                FD_ZERO(&fds);
                FD_SET(m_fd, &fds);

                tv.tv_sec = 1;
                tv.tv_usec = 0;

                ret = select(m_fd + 1, &fds, NULL, NULL, &tv);
            } while ((ret == -1 && (errno = EINTR)));

            if (ret == -1) {
                LogDebug(VB_PLUGIN, "Error in capture thread select: %s\n",
                    strerror(errno));
                m_captureFrames = false;
            }

            bzero(&buf, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            IOCTLWrapper(m_fd, VIDIOC_DQBUF, &buf);

            std::unique_lock<std::mutex> captureLock(m_captureMutex);

            LogExcess(VB_PLUGIN, "v4l2Capture: Captured frame #%llu\n", frameCount++);
            memcpy(m_frame, m_buffers[buf.index].start, buf.bytesused);

            IOCTLWrapper(m_fd, VIDIOC_QBUF, &buf);
        }

        LogDebug(VB_PLUGIN, "v4l2Capture: Capture thread done\n");
    });
}

void v4l2Capture::StopCapture()
{
    LogDebug(VB_PLUGIN, "v4l2Capture::StopCapture()\n");

    if (!m_captureFrames)
        return;

    m_captureFrames = false;
    m_captureThread->join();

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    IOCTLWrapper(m_fd, VIDIOC_STREAMOFF, &type);
}

void v4l2Capture::Close()
{
    LogDebug(VB_PLUGIN, "v4l2Capture::Close()\n");

    StopCapture();

    for (int i = 0; i < m_bufferCount; ++i)
        v4l2_munmap(m_buffers[i].start, m_buffers[i].length);

    v4l2_close(m_fd);

    free(m_frame);
}

bool v4l2Capture::GetFrame(unsigned char *buffer)
{
    LogExcess(VB_PLUGIN, "v4l2Capture::GetFrame(%p)\n", buffer);

    std::unique_lock<std::mutex> captureLock(m_captureMutex);
    memcpy(buffer, m_frame, m_width * m_height * 3);

    return true;
}

bool v4l2Capture::IOCTLWrapper(int fh, int request, void *arg)
{
    int ret;

    do {
        ret = v4l2_ioctl(fh, request, arg);
    } while (ret == -1 && ((errno == EINTR) || (errno == EAGAIN)));

    if (ret == -1) {
        LogErr(VB_PLUGIN, "v4l2_ioctl() failed: %s\n", strerror(errno));
        return false;
    }

    return true;
}
