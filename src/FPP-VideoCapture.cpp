#include <fpp-pch.h>




#include "Plugin.h"
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
        using namespace drogon;
        // /api/plugin-apis/VideoCapture/Cameras
        app().registerHandler("/api/plugin-apis/VideoCapture/Cameras",
            [this](const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) {
                Json::Value camerasJson;
                camerasJson["--Default--"] = std::string("--Default--");
                effect->ListCameras(camerasJson);
                auto resp = HttpResponse::newHttpJsonResponse(camerasJson);
                callback(resp);
            },
            {Post, Get});
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
