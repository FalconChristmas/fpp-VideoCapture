#include <fpp-pch.h>

#include <httpserver.hpp>


#include "Plugin.h"
#include "Plugins.h"

#include "VideoCaptureEffect.h"


class FPPVideoCapturePlugin : public FPPPlugin, public httpserver::http_resource {
public:
    
    FPPVideoCapturePlugin()
      : FPPPlugin("fpp-VideoCapture")
    {
        effect = VideoCaptureEffect::createVideoCaptureEffect();
    }

    virtual ~FPPVideoCapturePlugin() {
        delete effect;
    }

    virtual const std::shared_ptr<httpserver::http_response> render_GET(const httpserver::http_request &req) override {
        std::string respStr = "";
        int respCode = 404;
        std::string respType = "text/plain";
        if (req.get_path_pieces().size() > 1) {
            std::string p1 = req.get_path_pieces()[1];
            if (p1 == "Cameras") {
                Json::Value camerasJson;
                camerasJson["--Default--"] = std::string("--Default--");
                effect->ListCameras(camerasJson);
                respCode = 200;
                respStr = SaveJsonToString(camerasJson);
                respType = "application/json";
            }
        }
        return std::shared_ptr<httpserver::http_response>(new httpserver::string_response(respStr, respCode, respType));
    }

    void registerApis(httpserver::webserver *m_ws) override {
        m_ws->register_resource("/VideoCapture", this, true);
    }

    Json::Value      config;
    VideoCaptureEffect *effect = nullptr;
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPVideoCapturePlugin();
    }
}
