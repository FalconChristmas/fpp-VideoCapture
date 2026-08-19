#include <fpp-pch.h>

#include "fpphttp.h"
#include "common.h"

#include "Plugin.h"
#include "Plugins.h"
#include "overlays/PixelOverlay.h"
#include "overlays/PixelOverlayModel.h"

#include "VideoCaptureEffect.h"


class FPPVideoCapturePlugin : public FPPPlugin {
public:
    
    FPPVideoCapturePlugin()
      : FPPPlugin("fpp-VideoCapture")
    {
        effect = VideoCaptureEffect::createVideoCaptureEffect();
        ipEffect = new IPVideoCaptureEffect();
    }

    // Stop any capture still running before FPP destroys this plugin.
    //
    // This is the case that would otherwise make the plugin unsafe to unmap. A
    // started capture hands a RunningEffect - a class declared in this library,
    // with its own capture thread - to a PixelOverlayModel, which OWNS it and
    // will keep calling update() on it. That is the same shape as a plugin that
    // produced a ChannelOutput, which FPP refuses to unload for. The difference
    // is that this one can be taken back: setRunningEffect(nullptr) deletes the
    // effect, and its destructor stops and joins the capture thread.
    // updateRunningEffects() null-checks, so clearing it is safe.
    //
    // The effect objects themselves are unregistered from the overlay effect
    // registry by their own destructors, in ~FPPVideoCapturePlugin() below,
    // which runs before the library is unmapped.
    virtual std::function<bool()> shutdown() override {
        for (const auto &modelName : PixelOverlayManager::INSTANCE.getModelNames()) {
            PixelOverlayModel *m = PixelOverlayManager::INSTANCE.getModel(modelName);
            if (!m) {
                continue;
            }
            std::unique_lock<std::recursive_mutex> l(m->getRunningEffectMutex());
            RunningEffect *re = m->getRunningEffect();
            if (re && (re->name() == "Video Capture" || re->name() == "IP Video Capture")) {
                LogInfo(VB_PLUGIN, "Stopping %s on model %s before unload\n",
                        re->name().c_str(), modelName.c_str());
                m->setRunningEffect(nullptr, 0); // deletes it, joining its thread
            }
        }
        return nullptr;
    }

    virtual ~FPPVideoCapturePlugin() {
        delete effect;
        delete ipEffect;
    }

    void handleVideoCaptureRequest(const HttpRequestPtr &req, HttpCallback &&callback) {
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
        // The web UI requests /VideoCapture/Cameras (proxied from
        // api/plugin-apis/VideoCapture/Cameras), so family=true covers the
        // subpaths as well as the bare path.
        //
        // Registered through FPP rather than drogon::app() directly: drogon has
        // no route removal, so a handler registered straight with it could never
        // be withdrawn and would pin this plugin in memory for the life of fppd.
        FPPPlugins::registerPluginApi(
            "/VideoCapture",
            [this](const HttpRequestPtr &req, HttpCallback &&callback) {
                handleVideoCaptureRequest(req, std::move(callback));
            },
            {drogon::Get}, true);
    }

    void unregisterApis() override {
        // Does not return until no request is inside the handler and the
        // handler itself has been destroyed, which is what makes a later
        // dlclose() safe.
        FPPPlugins::unregisterPluginApi("/VideoCapture");
    }

    Json::Value      config;
    VideoCaptureEffect *effect = nullptr;
    IPVideoCaptureEffect *ipEffect = nullptr;
};


// Safe to dlclose() on unload: no threads of its own, no timers, no CurlManager
// requests, no epoll descriptors, no commands and no drogon client objects. The
// route goes through registerPluginApi() and comes back in unregisterApis();
// shutdown() stops any capture still running, so no RunningEffect declared here
// is left owned by an overlay model; and the effect objects unregister
// themselves from the overlay effect registry in the destructor.
FPP_PLUGIN_SUPPORTS_UNLOAD()

extern "C" {
    FPPPlugin *createPlugin() {
        return new FPPVideoCapturePlugin();
    }
}
