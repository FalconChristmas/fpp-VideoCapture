

#include "overlays/PixelOverlayEffects.h"
#include "overlays/PixelOverlayModel.h"

class VideoCaptureEffect : public PixelOverlayEffect {
public:
    VideoCaptureEffect() : PixelOverlayEffect("Video Capture") {
        args.push_back(CommandArg("Camera", "string", "Camera").setContentListUrl("api/plugin-apis/VideoCapture/Cameras", false));
    }

    virtual ~VideoCaptureEffect() {

    }


    virtual void ListCameras(Json::Value &resp) {}



    static VideoCaptureEffect *createVideoCaptureEffect();
};