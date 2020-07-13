#include <atomic>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cstring>

#include <unistd.h>
#include <termios.h>
#include <chrono>
#include <thread>
#include <cmath>

#include <httpserver.hpp>

#include "channeloutput/channeloutputthread.h"
#include "commands/Commands.h"
#include "common.h"
#include "Plugin.h"
#include "Plugins.h"
#include "Sequence.h"
#include "log.h"

#include "v4l2Capture.h"

class FPPVideoCapturePlugin : public FPPPlugin, public httpserver::http_resource {
public:
    
    FPPVideoCapturePlugin()
      : FPPPlugin("fpp-VideoCapture"),
        vcap(NULL),
        width(160),
        height(120),
        outputWidth(160),
        outputHeight(120),
        cropL(0),
        cropT(0),
        startChannel(0),
        captureOn(true),
        forceChannelOutput(false),
        tmpFrame(NULL)
    {
        std::string  deviceName = "/dev/video0";
        unsigned int fps = 5;

        if (LoadJsonFromFile("/home/fpp/media/config/plugin.fpp-VideoCapture.json", config)) {
            if (config.isMember("deviceName"))
                deviceName = config["deviceName"].asString();

            if (config.isMember("width"))
                width = config["width"].asInt();

            if (config.isMember("height"))
                height = config["height"].asInt();

            if (config.isMember("outputWidth"))
                outputWidth = config["outputWidth"].asInt();

            if (config.isMember("outputHeight"))
                outputHeight = config["outputHeight"].asInt();

            if (config.isMember("cropL"))
                cropL = config["cropL"].asInt();

            if (config.isMember("cropT"))
                cropT = config["cropT"].asInt();

            if (config.isMember("fps"))
                fps = config["fps"].asInt();

            if (config.isMember("startChannel")) // UI is 1-based, we are 0
                startChannel = config["startChannel"].asInt() - 1;

            if (config.isMember("captureOn"))
                captureOn = (config["captureOn"].asInt() == 1);

            if (config.isMember("forceChannelOutput"))
                forceChannelOutput = (config["forceChannelOutput"].asInt() == 1);
        }

        tmpFrame = (unsigned char *)malloc(width * height * 3);

        if ((startChannel + (width * height * 3)) < FPPD_MAX_CHANNELS) {
            if (FileExists(deviceName)) {
                vcap = new v4l2Capture(deviceName.c_str(), width, height, fps);

                if (vcap && !vcap->Init()) {
                    delete vcap;
                    vcap = nullptr;
                }
            } else {
                LogErr(VB_PLUGIN, "Device %s does not exist\n", deviceName.c_str());
            }
        } else {
            LogErr(VB_PLUGIN, "Video buffer extends past max channel value\n");
        }

        registerCommand();

        if (forceChannelOutput)
            StartForcingChannelOutput();
    }

    virtual ~FPPVideoCapturePlugin() {
        if (forceChannelOutput)
            StopForcingChannelOutput();

        if (vcap) {
            delete vcap;
            vcap = nullptr;
        }

        if (tmpFrame)
            free(tmpFrame);
    }

    class SetVideoCaptureCommand : public Command {
    public:
        SetVideoCaptureCommand(FPPVideoCapturePlugin *p) : Command("VideoCapture"), plugin(p) {
            args.push_back(CommandArg("capture", "bool", "Capture Video")
                           .setDefaultValue("true"));
        }

        virtual std::unique_ptr<Command::Result> run(const std::vector<std::string> &args) override {
            std::string v = args[0];
            if ((v == "true") || (v == "1")) {
                plugin->captureOn = true;
            } else {
                plugin->captureOn = false;
            }
            return std::make_unique<Command::Result>("OK");
        }

        FPPVideoCapturePlugin *plugin;
    };
    void registerCommand() {
        CommandManager::INSTANCE.addCommand(new SetVideoCaptureCommand(this));
    }

    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override {
        std::string respStr = "{ \"Status\": \"OK\" }";
        int respCode = 200;
        std::string respType = "application/json";
        std::string p0 = req.get_path_pieces()[0];
        int plen = req.get_path_pieces().size();
        if (plen > 0) {
            if (req.get_path_pieces()[1] == "getFrame") {
                if (vcap) {
                    int pixels = width * height;

                    respType = "image/x-portable-pixmap";

                    vcap->GetFrame(tmpFrame);

                    char line[32];
                    sprintf(line, "P3\n%d %d\n255\n", width, height);

                    respStr = line;

                    unsigned char *r = tmpFrame;
                    unsigned char *g = tmpFrame + 1;
                    unsigned char *b = tmpFrame + 2;

                    for (int i = 0; i < pixels; i++) {
                        sprintf(line, "%d %d %d\n", (int)*r, (int)*g, (int)*b);
                        respStr += line;

                        r += 3;
                        g += 3;
                        b += 3;
                    }
                } else {
                    respStr = "{ \"Status\": \"No Capture Device Configured\" }";
                    respCode = 404;
                }
            }
        }

        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(respStr, respCode, respType));
    }

    void registerApis(httpserver::webserver *m_ws) override {
        m_ws->register_resource("/VideoCapture", this, true);
    }

    virtual void modifyChannelData(int ms, uint8_t *seqData) override {
        LogExcess(VB_PLUGIN, "modifyChannelData()\n");

        if (vcap && captureOn) {
            if ((width != outputWidth) || (height != outputHeight)) {
                if (vcap->GetFrame(tmpFrame)) {
                    unsigned int stride = outputWidth * 3;
                    unsigned char *src = tmpFrame;
                    unsigned char *dst = seqData + startChannel;

                    for (int y = 0; y < outputHeight; y++) {
                        src = tmpFrame + ((y + cropT) * width * 3) + (cropL * 3);
                        memcpy(dst, src, stride);
                        dst += stride;
                    }
                }
            } else {
                vcap->GetFrame(seqData + startChannel);
            }
        }
    }

    Json::Value      config;
    v4l2Capture     *vcap;
    unsigned int     width;
    unsigned int     height;
    unsigned int     outputWidth;
    unsigned int     outputHeight;
    unsigned int     cropL;
    unsigned int     cropT;
    unsigned int     startChannel;
    std::atomic_bool captureOn;
    bool             forceChannelOutput;
    unsigned char   *tmpFrame;
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPVideoCapturePlugin();
    }
}
