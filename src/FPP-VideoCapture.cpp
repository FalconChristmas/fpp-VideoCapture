#include <fpp-pch.h>

// HttpAppFramework.h must come before fpphttp.h: fpphttp.h undefines the
// trantor LOG_* macros, but drogon's own headers need them while compiling.
#include <drogon/HttpAppFramework.h>
#include "fpphttp.h"


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

    void handleVideoCaptureRequest(const drogon::HttpRequestPtr &req,
                                   std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
        std::string respStr = "";
        int respCode = 404;
        std::string respType = "text/plain";
        std::vector<std::string> pieces = getPathPieces(req->path());
        if (pieces.size() > 1) {
            std::string p1 = pieces[1];
            if (p1 == "Cameras") {
                Json::Value camerasJson;
                camerasJson["--Default--"] = std::string("--Default--");
                effect->ListCameras(camerasJson);
                respCode = 200;
                respStr = SaveJsonToString(camerasJson);
                respType = "application/json";
            }
        }
        callback(makeStringResponse(respStr, respCode, respType));
    }

    void registerApis() override {
        auto handler = [this](const drogon::HttpRequestPtr &req,
                              std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            handleVideoCaptureRequest(req, std::move(callback));
        };
        auto handler2 = handler;
        // The web UI requests /VideoCapture/Cameras (proxied from
        // api/plugin-apis/VideoCapture/Cameras), so register the subpath regex
        // in addition to the bare path.
        drogon::app().registerHandler("/VideoCapture", std::move(handler), {drogon::Get});
        drogon::app().registerHandlerViaRegex("/VideoCapture/.*", std::move(handler2), {drogon::Get});
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
