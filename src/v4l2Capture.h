#ifndef _V4L2CAPTURE_H_
#define _V4L2CAPTURE_H_

#include <atomic>
#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <string.h>
#include <thread>

#include <linux/videodev2.h>
#include "libv4l2.h"

class v4l2Capture {
  public:
    v4l2Capture(std::string deviceName, unsigned int width, unsigned int height, unsigned int fps);
    ~v4l2Capture();

    bool Init();

    bool GetFrame(unsigned char *buffer);

  private:
    struct v4l2Buffer {
        void   *start;
        size_t  length;
    };

    void StartCapture();
    void StopCapture();
    void Close();

    bool IOCTLWrapper(int fh, int request, void *arg);

    std::string                     m_deviceName;
    unsigned int                    m_width;
    unsigned int                    m_height;
    unsigned int                    m_fps;

    int                             m_fd;
    unsigned int                    m_bufferCount;
    struct v4l2Buffer              *m_buffers;

    std::mutex                      m_captureMutex;
    std::thread                    *m_captureThread;
    std::atomic_bool                m_captureFrames;
    unsigned char                  *m_frame;
};

#endif
