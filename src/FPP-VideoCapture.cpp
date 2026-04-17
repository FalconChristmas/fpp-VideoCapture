#include <fpp-pch.h>




#include "Plugin.h"
#include <drogon/drogon.h>
#include "Plugins.h"

#include "VideoCaptureEffect.h"


class FPPVideoCapturePlugin : public FPPPlugin {
public:
    
    FPPVideoCapturePlugin()
      : FPPPlugin("fpp-VideoCapture")
    {
        effect = VideoCaptureEffect::createVideoCaptureEffect();
        ipEffect = new IPVideoCaptureEffect();
    }

    virtual ~FPPVideoCapturePlugin() {
        delete effect;
        delete ipEffect;
    }


    // Register drogon HTTP API endpoints
    void registerApis() override {
        // /api/plugin-apis/VideoCapture/Cameras
        drogon::app().registerHandler("/api/plugin-apis/VideoCapture/Cameras",
            [this](const drogon::HttpRequestPtr& req, std::function<void (const drogon::HttpResponsePtr &)> &&callback) {
                Json::Value camerasJson;
                camerasJson["--Default--"] = std::string("--Default--");
                effect->ListCameras(camerasJson);
                auto resp = drogon::HttpResponse::newHttpJsonResponse(camerasJson);
                callback(resp);
            },
            {drogon::Post, drogon::Get});
    }



    Json::Value      config;
    VideoCaptureEffect *effect = nullptr;
    IPVideoCaptureEffect *ipEffect = nullptr;
};


extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPVideoCapturePlugin();
    }
}
