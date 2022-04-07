#ifndef _AMINO_RPI_H
#define _AMINO_RPI_H

#include "base.h"
#include "renderer.h"

//video
#include "rpi_video.h"

//VideoCore
#include "bcm_host.h"

//Dispmanx
#ifdef EGL_DISPMANX
#include "interface/vchiq_arm/vchiq_if.h"
#endif

//GBM
#ifdef EGL_GBM
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#endif

#include <semaphore.h>

//input
#include "rpi_input.h"

class AminoGfxRPiFactory : public AminoJSObjectFactory {
public:
    AminoGfxRPiFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Raspberry Pi AminoGfx implementation.
 */
class AminoGfxRPi : public AminoGfx {
public:
    AminoGfxRPi();
    ~AminoGfxRPi();

    static AminoGfxRPiFactory* getFactory();
    static NAN_MODULE_INIT(Init);

    EGLImageKHR createEGLImage(GLuint textureId);
    void destroyEGLImage(EGLImageKHR eglImage);

private:
    static bool glESInitialized;

    //OpenGL ES
    NativeDisplayType displayType = EGL_DEFAULT_DISPLAY;
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLConfig config;
    uint32_t screenW = 0;
    uint32_t screenH = 0;

#ifdef EGL_GBM
    //DRM/GBM
    int driDevice = 0;
    uint32_t connector_id = 0;
    drmModeModeInfo mode_info;
    gbm_surface *gbmSurface = NULL;
    drmModeCrtc *crtc = NULL;
    gbm_bo *previous_bo = NULL;
    std::map<uint32_t, uint32_t> fbCache;
    bool pageFlipPending = false;
#endif

    //resolution
    std::string prefRes = "";

    //display
    std::string prefDisp = "";

    //touch
    bool touchInvertX = false;
    bool touchInvertY = false;

#ifdef EGL_DISPMANX
    static sem_t resSem;
    static bool resSemValid;
#endif

    //input
    std::vector<AminoInputRPi *> inputDevices;

    static NAN_METHOD(New);

    void setup() override;
    void setupTouch(v8::Local<v8::Object> &touch);
    void initEGL();

    static TV_DISPLAY_STATE_T* getDisplayState();

#ifdef EGL_DISPMANX
    static void tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2);
#endif

    void destroy() override;
    void destroyAminoGfxRPi();

    bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) override;
    void getStats(v8::Local<v8::Object> &obj) override;
#ifdef EGL_GBM
    void getDrmStats(v8::Local<v8::Object> &obj);
    void showPropertyBlob(uint32_t id, char *name);
#endif

#ifdef EGL_DISPMANX
    void forceHdmiMode(uint32_t code);
    void switchHdmiOff();
#endif

    void populateRuntimeProperties(v8::Local<v8::Object> &obj) override;
    void initRenderer() override;

#ifdef EGL_DISPMANX
    EGLSurface createDispmanxSurface();
#endif

#ifdef EGL_GBM
    EGLSurface createGbmSurface();
    std::string getDrmConnectorType(drmModeConnector *connector);
    std::string getDrmSubpixelMode(drmModeSubPixel subpixel);
    std::string getDrmModeTypes(uint32_t type);
    std::string getDrmModeFlags(uint32_t flags);
    static void handlePageFlipEvent(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
#endif

    bool startsWith(const char *pre, const char *str);
    void initInput();
    void initTouch(int fd);

    void start() override;
    bool bindContext() override;
    void renderingDone() override;
    void handleSystemEvents() override;

    void processInputs();

    void updateWindowSize() override;
    void updateWindowPosition() override;
    void updateWindowTitle() override;

    void atlasTextureHasChanged(texture_atlas_t *atlas) override;
    void atlasTextureHasChangedHandler(JSCallbackUpdate *update);

    AminoVideoPlayer *createVideoPlayer(AminoTexture *texture, AminoVideo *video) override;

    void destroyEGLImageHandler(AsyncValueUpdate *update, int state);

    //access rights
    friend class AminoInputRPi;
};

#endif
