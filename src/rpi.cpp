#include "rpi.h"

#include <dirent.h>

#include <execinfo.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include <sstream>

#define gettid() syscall(SYS_gettid)

//debug
#define DEBUG_GLES false
#define DEBUG_RENDER false
#define DEBUG_HDMI false

#define USE_DRM_PAGEFLIP false

#define AMINO_EGL_SAMPLES 4

//
// AminoGfxRPi
//

AminoGfxRPi::AminoGfxRPi(): AminoGfx(getFactory()->name) {
    //empty
}

AminoGfxRPi::~AminoGfxRPi() {
    if (!destroyed) {
        destroyAminoGfxRPi();
    }
}

/**
 * Get factory instance.
 */
AminoGfxRPiFactory* AminoGfxRPi::getFactory() {
    static AminoGfxRPiFactory *instance = NULL;

    if (!instance) {
        instance = new AminoGfxRPiFactory(New);
    }

    return instance;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoGfxRPi::Init) {
    AminoGfxRPiFactory *factory = getFactory();

    AminoGfx::Init(target, factory);
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoGfxRPi::New) {
    AminoJSObject::createInstance(info, getFactory());
}

/**
 * Setup JS instance.
 */
void AminoGfxRPi::setup() {
    if (DEBUG_GLES) {
        printf("AminoGfxRPi.setup()\n");
    }

    //init OpenGL ES
    if (!glESInitialized) {
        //Note: needed in GBM case too to access VC6 APIs
        if (DEBUG_GLES) {
            printf("-> initializing VideoCore\n");
        }

#ifdef EGL_DISPMANX
        //VideoCore IV
        //Note: was supported on Buster but removed on Bullseye
        bcm_host_init();
#endif

#ifdef EGL_GBM
        if (DEBUG_GLES) {
            printf("-> initializing OpenGL driver\n");
        }

        //TODO how to use the dual display output?

        //access OpenGL driver (available if OpenGL driver is loaded)
        //Note: Pi0-3 use /dev/dri/card0
        std::string devicePath = "/dev/dri/card1";

        driDevice = open(devicePath.c_str(), O_RDWR | O_CLOEXEC);

        assert(driDevice > 0);

        if (DEBUG_GLES) {
            printf("-> DRI device ready: %s\n", devicePath.c_str());
        }

        /*
         * Sometimes getting this issue after rebooting a device.
         *
         *  - /dev/dri/card1 fails but HDMI should always be initialized
         *  - workaround:
         *    - https://github.com/alanbork/rpi-opengl-without-x/blob/master/triangle_rpi4.c#L149
         *  - articles:
         *    - https://forums.raspberrypi.com/viewtopic.php?t=291199
         *
         * -> have to verify DRM first
         */
        drmModeRes *resources = drmModeGetResources(driDevice);

        if (resources == NULL) {
            close(driDevice);

            printf("-> %s has no DRM resources!\n", devicePath.c_str());

            //fallback to card0
            devicePath = "/dev/dri/card0";
            driDevice = open(devicePath.c_str(), O_RDWR | O_CLOEXEC);

            printf("-> trying %s\n", devicePath.c_str());

            assert(driDevice > 0);

            if (DEBUG_GLES) {
                printf("-> DRI device ready: %s\n", devicePath.c_str());
            }

            //get resources
            resources = drmModeGetResources(driDevice);

            if (resources != NULL) {
                printf("-> %s available.\n", devicePath.c_str());
            }

            assert(resources);
        }

        drmModeFreeResources(resources);
#endif
        //Note: tvservice and others are already initialized by bcm_host_init() call!
        //      see https://github.com/raspberrypi/userland/blob/master/host_applications/linux/libs/bcm_host/bcm_host.c

        if (DEBUG_GLES) {
            printf("-> ready\n");
        }

#ifdef EGL_DISPMANX
        /*
         * register callback
         *
         * Note: never called with "hdmi_force_hotplug=1".
         */
        vc_tv_register_callback(tvservice_cb, NULL);
#endif

        //show info screen (Note: seems not to work!)
        //vc_tv_show_info(1);

        //handle preferred resolution
        if (!createParams.IsEmpty()) {
            v8::Local<v8::Object> obj = Nan::New(createParams);

            //resolution
            Nan::MaybeLocal<v8::Value> resolutionMaybe = Nan::Get(obj, Nan::New<v8::String>("resolution").ToLocalChecked());

            if (!resolutionMaybe.IsEmpty()) {
                v8::Local<v8::Value> resolutionValue = resolutionMaybe.ToLocalChecked();

                if (resolutionValue->IsString()) {
                    prefRes = AminoJSObject::toString(resolutionValue);

#ifdef EGL_DISPMANX
                    //change resolution
                    if (prefRes == "720p@24") {
                        forceHdmiMode(HDMI_CEA_720p24);
                    } else if (prefRes == "720p@25") {
                        forceHdmiMode(HDMI_CEA_720p25);
                    } else if (prefRes == "720p@30") {
                        forceHdmiMode(HDMI_CEA_720p30);
                    } else if (prefRes == "720p@50") {
                        forceHdmiMode(HDMI_CEA_720p50);
                    } else if (prefRes == "720p@60") {
                        forceHdmiMode(HDMI_CEA_720p60);
                    } else if (prefRes == "1080p@24") {
                        forceHdmiMode(HDMI_CEA_1080p24);
                    } else if (prefRes == "1080p@25") {
                        forceHdmiMode(HDMI_CEA_1080p25);
                    } else if (prefRes == "1080p@30") {
                        forceHdmiMode(HDMI_CEA_1080p30);
                    } else if (prefRes == "1080p@50") {
                        forceHdmiMode(HDMI_CEA_1080p50);
                    } else if (prefRes == "1080p@60") {
                        forceHdmiMode(HDMI_CEA_1080p60);
                    } else {
                        printf("unknown resolution: %s\n", prefRes.c_str());
                    }
#endif
                }
            }

            //display
            Nan::MaybeLocal<v8::Value> displayMaybe = Nan::Get(obj, Nan::New<v8::String>("display").ToLocalChecked());

            if (!displayMaybe.IsEmpty()) {
                v8::Local<v8::Value> displayValue = displayMaybe.ToLocalChecked();

                if (displayValue->IsString()) {
                    prefDisp = AminoJSObject::toString(displayValue);
                }
            }

            //touch
            Nan::MaybeLocal<v8::Value> touchMaybe = Nan::Get(obj, Nan::New<v8::String>("touch").ToLocalChecked());

            if (!touchMaybe.IsEmpty()) {
                //debug
                //printf("-> has touch properties\n");

                //get properties
                v8::Local<v8::Value> touchLocal = touchMaybe.ToLocalChecked();

                //debug
                //Nan::Utf8String str(Nan::ToDetailString(touchLocal).ToLocalChecked());
                //printf("-> %s\n", *str);

                if (touchLocal->IsObject()) {
                    v8::Local<v8::Object> touchObj = Nan::To<v8::Object>(touchLocal).ToLocalChecked();

                    setupTouch(touchObj);
                }
            }
        }

        glESInitialized = true;
    }

    //instance
    addInstance();

    //basic EGL to get screen size
    initEGL();

    //base class
    AminoGfx::setup();
}

void AminoGfxRPi::setupTouch(v8::Local<v8::Object> &touch) {
    //invertX
    Nan::MaybeLocal<v8::Value> invertXMaybe = Nan::Get(touch, Nan::New<v8::String>("invertX").ToLocalChecked());

    if (!invertXMaybe.IsEmpty()) {
        v8::Local<v8::Value> invertXValue = invertXMaybe.ToLocalChecked();

        if (invertXValue->IsBoolean()) {
            touchInvertX = Nan::To<v8::Boolean>(invertXValue).ToLocalChecked()->Value();
        }
    }

    //invertY
    Nan::MaybeLocal<v8::Value> invertYMaybe = Nan::Get(touch, Nan::New<v8::String>("invertY").ToLocalChecked());

    if (!invertYMaybe.IsEmpty()) {
        v8::Local<v8::Value> invertYValue = invertYMaybe.ToLocalChecked();

        if (invertYValue->IsBoolean()) {
            touchInvertY = Nan::To<v8::Boolean>(invertYValue).ToLocalChecked()->Value();
        }
    }

    //debug
    //printf("-> invertX=%d invertY=%d\n", touchInvertX, touchInvertY);
}

/**
 * Initialize EGL and get display size.
 */
void AminoGfxRPi::initEGL() {
    if (DEBUG_GLES) {
        printf("AminoGfxRPi::initEGL()\n");
    }

#ifdef EGL_GBM
    //create GBM device (Note: fails if not enough rights e.g. no root access)
    displayType = gbm_create_device(driDevice);

    if (!displayType) {
        printf("Could not create the GBM device! Please check your permissions (e.g. run with root privileges).\n");
    }

    assert(displayType);

    if (DEBUG_GLES) {
        printf("-> created GBM device\n");
    }
#endif

    //get an EGL display connection
    display = eglGetDisplay(displayType);

    assert(display != EGL_NO_DISPLAY);

    if (DEBUG_GLES) {
        printf("-> got EGL display\n");
    }

    //initialize the EGL display connection
    EGLBoolean res = eglInitialize(display, NULL, NULL);

    assert(res != EGL_FALSE);

    if (DEBUG_GLES) {
        printf("-> EGL initialized\n");
    }

    //get an appropriate EGL frame buffer configuration
    static const EGLint attribute_list[] = {
        //RGBA
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,

#ifdef EGL_GBM
        //using GBM_FORMAT_XRGB8888
        EGL_ALPHA_SIZE, 0,
#else
        //use Dispmanx transparency effect (equal to GBM_FORMAT_ARGB8888)
        EGL_ALPHA_SIZE, 8,
#endif

        //OpenGL ES 2.0
        EGL_CONFORMANT, EGL_OPENGL_ES2_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,

        //buffers
        EGL_STENCIL_SIZE, 8,
        EGL_DEPTH_SIZE, 16,

        //sampling (quality)
        //Note: does not work on RPi 4 (GBM) -> no config
#ifdef EGL_DISPMANX
        EGL_SAMPLE_BUFFERS, 1,
        EGL_SAMPLES, AMINO_EGL_SAMPLES, //4: 4x MSAA
#endif

        //window
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,

        EGL_NONE
    };

#ifdef EGL_DISPMANX
    EGLint num_config;

    //this uses a BRCM extension that gets the closest match, rather than standard which returns anything that matches
    res = eglSaneChooseConfigBRCM(display, attribute_list, &config, 1, &num_config);

    assert(EGL_FALSE != res);
#endif

#ifdef EGL_GBM
    //get config count
    EGLint count = 0;

    res = eglGetConfigs(display, NULL, 0, &count);

    assert(EGL_FALSE != res);

    //get configs
    EGLConfig *configs = (EGLConfig *)malloc(count * sizeof *configs);

    res = eglChooseConfig(display, attribute_list, configs, count, &count);

    assert(EGL_FALSE != res);
    assert(count > 0);

    if (DEBUG_GLES) {
        printf("-> configs found: %i\n", count);
    }

    //find matching config
    int pos = -1;
    EGLint wantedId = GBM_FORMAT_XRGB8888;

    for (int i = 0; i < count; i++) {
        if (DEBUG_GLES) {
            EGLint value;

            if (eglGetConfigAttrib(display, configs[i], EGL_SAMPLE_BUFFERS, &value)) {
                printf("-> EGL_SAMPLE_BUFFERS: %i\n", value);
            }

            if (eglGetConfigAttrib(display, configs[i], EGL_SAMPLES, &value)) {
                printf("-> EGL_SAMPLES: %i\n", value);
            }
        }

        EGLint id;

        if (eglGetConfigAttrib(display, configs[i], EGL_NATIVE_VISUAL_ID, &id) == EGL_FALSE) {
            continue;
        }

        if (DEBUG_GLES) {
            printf("-> format: %i %c%c%c%c\n", id, (char)(id & 0xFF), (char)((id >> 8) & 0xFF), (char)((id >> 16) & 0xFF), (char)((id >> 24) & 0xFF));
        }

        if (id == wantedId) {
            pos = i;
            break;
        }
    }

    assert(pos != -1);

    config = configs[pos];
    free(configs);
#endif

    //choose OpenGL ES 2
    res = eglBindAPI(EGL_OPENGL_ES_API);

    assert(EGL_FALSE != res);

    //create an EGL rendering context
    static const EGLint context_attributes[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);

    assert(context != EGL_NO_CONTEXT);

#ifdef EGL_DISPMANX
    //get display size (see http://elinux.org/Raspberry_Pi_VideoCore_APIs#graphics_get_display_size)
    int32_t success = graphics_get_display_size(0 /* LCD */, &screenW, &screenH);

    assert(success >= 0); //Note: check display resolution (force if not connected to display)
#endif

#ifdef EGL_GBM
    //get display resolutions
    drmModeRes *resources = drmModeGetResources(driDevice);

    assert(resources);

    //find connector
    drmModeConnector *connector = NULL;

    if (DEBUG_GLES) {
        printf("DRM connectors: %i\n", resources->count_connectors);

        //list all connectors
        for (int i = 0; i < resources->count_connectors; i++) {
            drmModeConnector *connector2 = drmModeGetConnector(driDevice, resources->connectors[i]);

            if (!connector2) {
                continue;
            }

            //name and status
            std::string type = getDrmConnectorType(connector2);
            std::string connected = connector2->connection == DRM_MODE_CONNECTED ? "connected":"disconnected";

            printf(" -> %s-%d (%s)\n", type.c_str(), connector2->connector_type_id, connected.c_str());

            //modes
            for (int i = 0; i < connector2->count_modes; i++) {
                drmModeModeInfo mode = connector2->modes[i];

                printf("  -> %ix%i@%i (%s)\n", mode.hdisplay, mode.vdisplay, mode.vrefresh, mode.name);
            }

            //done
            drmModeFreeConnector(connector2);
        }
    }

    for (int i = 0; i < resources->count_connectors; i++) {
        drmModeConnector *connector2 = drmModeGetConnector(driDevice, resources->connectors[i]);

        if (!connector2) {
            continue;
        }

        //pick the first connected connector
        if (connector2->connection == DRM_MODE_CONNECTED && connector2->count_modes > 0) {
            bool match = true;

            //check display
            if (!prefDisp.empty()) {
                std::string name(getDrmConnectorType(connector2) + "-" + std::to_string(connector2->connector_type_id));

                match = name == prefDisp;

                if (DEBUG_GLES) {
                    //debug
                    //printf("-> checking display %s %s\n", name.c_str(), prefDisp.c_str());

                    if (match) {
                        printf("-> using display %s\n", prefDisp.c_str());
                    }
                }
            }

            if (match) {
                //Note: have to free instance later
                connector = connector2;
                break;
            }
        }

        drmModeFreeConnector(connector2);
    }

    assert(connector);

    connector_id = connector->connector_id;

    //select mode
    int prefH = 0;
    uint32_t prefRefresh = 0;

    if (prefRes != "") {
        //supported aminogfx values
        if (prefRes == "720p@24") {
            prefH = 720;
            prefRefresh = 24;
        } else if (prefRes == "720p@25") {
            prefH = 720;
            prefRefresh = 25;
        } else if (prefRes == "720p@30") {
            prefH = 720;
            prefRefresh = 30;
        } else if (prefRes == "720p@50") {
            prefH = 720;
            prefRefresh = 50;
        } else if (prefRes == "720p@60") {
            prefH = 720;
            prefRefresh = 60;
        } else if (prefRes == "1080p@24") {
            prefH = 1080;
            prefRefresh = 24;
        } else if (prefRes == "1080p@25") {
            prefH = 1080;
            prefRefresh = 25;
        } else if (prefRes == "1080p@30") {
            prefH = 1080;
            prefRefresh = 30;
        } else if (prefRes == "1080p@50") {
            prefH = 1080;
            prefRefresh = 50;
        } else if (prefRes == "1080p@60") {
            prefH = 1080;
            prefRefresh = 60;
        } else {
            printf("unknown resolution: %s\n", prefRes.c_str());
        }
    }

    if (prefH) {
        /*
         * Issues:
         *
         *   - without attached display only offers the default set display resolution
         *     - i.e. 720p would fail if 1080p is the default
         *     - Workarounds:
         *       - modify default resolution in /boot/config.txt
         *       - always attach a display
         */

        //show all modes
        if (DEBUG_GLES || DEBUG_HDMI) {
            printf("-> modes: %i\n", connector->count_modes);

            //show all
            for (int i = 0; i < connector->count_modes; i++) {
                drmModeModeInfo mode = connector->modes[i];

                printf(" -> %ix%i@%i (%s)\n", mode.hdisplay, mode.vdisplay, mode.vrefresh, mode.name);
            }
        }

        //find matching resolution
        bool found = false;

        for (int i = 0; i < connector->count_modes; i++) {
            drmModeModeInfo mode = connector->modes[i];

            if (mode.vdisplay == prefH && mode.vrefresh == prefRefresh) {
                mode_info = mode;
                found = true;
                break;
            }
        }

        //check
        if (!found) {
            printf("No matching resolution found!\n");

            prefH = 0;
            prefRefresh = 0;
        }
    }

    if (!prefH) {
        //use first mode
        mode_info = connector->modes[0];
    }

    //show mode
    if (DEBUG_GLES || DEBUG_HDMI) {
        printf(" -> using: %ix%i@%i (%s)\n", mode_info.hdisplay, mode_info.vdisplay, mode_info.vrefresh, mode_info.name);
    }

    screenW = mode_info.hdisplay;
    screenH = mode_info.vdisplay;

    //find an encoder
    assert(connector->encoder_id);

    drmModeEncoder *encoder = drmModeGetEncoder(driDevice, connector->encoder_id);

    assert(encoder);

    //find a CRTC
    if (encoder->crtc_id) {
        crtc = drmModeGetCrtc(driDevice, encoder->crtc_id);

        assert(crtc);
        assert(crtc->crtc_id);
    }

    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);
#endif

    assert(screenW > 0);
    assert(screenH > 0);

    if (DEBUG_GLES) {
        printf("RPI: display size = %d x %d\n", screenW, screenH);
    }
}

/**
 * Get the current display state.
 *
 * Returns NULL if no display is connected.
 */
TV_DISPLAY_STATE_T* AminoGfxRPi::getDisplayState() {
    /*
     * Get TV state.
     *
     * - https://github.com/bmx-ng/sdl.mod/blob/master/sdlgraphics.mod/rpi_glue.c
     * - https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h
     */
    TV_DISPLAY_STATE_T *tvstate = (TV_DISPLAY_STATE_T *)malloc(sizeof(TV_DISPLAY_STATE_T));

    assert(tvstate);

    if (vc_tv_get_display_state(tvstate) != 0) {
        free(tvstate);
        tvstate = NULL;
    } else {
        /*
         * Notes:
         *
         * - Raspberry Pi 3 default:
         *    - hdmi_force_hotplug=1 is being used in /boot/config.txt
         *    - 1920x1080@60Hz on HDMI (mode=16, group=1)
         *    - monitor data gets cached, reported as connected even if the HDMI cable was unplugged!
         */

        //debug
        if (DEBUG_HDMI) {
            printf("State: %i\n", tvstate->state);
        }

        //check HDMI
        if ((tvstate->state & VC_HDMI_UNPLUGGED) == VC_HDMI_UNPLUGGED) {
            //unplugged (so notes above)
            if (DEBUG_HDMI) {
                printf("-> unplugged\n");
            }

            free(tvstate);
            tvstate = NULL;
        } else if ((tvstate->state & (VC_HDMI_HDMI | VC_HDMI_DVI)) == 0) {
            //no HDMI (or DVI) output
            if (DEBUG_HDMI) {
                printf("-> no HDMI display\n");
            }

            free(tvstate);
            tvstate = NULL;
        }

        if (tvstate && DEBUG_HDMI) {
            printf("Currently outputting %ix%i@%iHz on HDMI (mode=%i, group=%i).\n", tvstate->display.hdmi.width, tvstate->display.hdmi.height, tvstate->display.hdmi.frame_rate, tvstate->display.hdmi.mode, tvstate->display.hdmi.group);
        }
    }

    return tvstate;
}

#ifdef EGL_DISPMANX
/**
 * HDMI tvservice callback (Dispmanx implementation).
 */
void AminoGfxRPi::tvservice_cb(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2) {
    //http://www.m2x.nl/videolan/vlc/blob/1d2b56c68bbc3287e17f6140bdf8c8c3efe08fdc/modules/hw/mmal/vout.c

    if (DEBUG_HDMI) {
        //reasons seen: VC_HDMI_HDMI
        printf("-> tvservice state has changed: %s\n", vc_tv_notification_name((VC_HDMI_NOTIFY_T)reason));
    }

    //check resolution
    TV_DISPLAY_STATE_T *tvState = getDisplayState();

    if (tvState) {
        //Note: new resolution used by Dispmanx calls
        free(tvState);
    }

    //check sem
    if (resSemValid) {
        sem_post(&resSem);
    }
}
#endif

/**
 * Destroy GLFW instance.
 */
void AminoGfxRPi::destroy() {
    if (destroyed) {
        return;
    }

    //instance
    destroyAminoGfxRPi();

    //destroy basic instance (activates context)
    AminoGfx::destroy();
}

/**
 * Destroy EGL instance.
 */
void AminoGfxRPi::destroyAminoGfxRPi() {
    //OpenGL ES
    if (display != EGL_NO_DISPLAY) {
#ifdef EGL_GBM
        //set the previous crtc
        drmModeSetCrtc(driDevice, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y, &connector_id, 1, &crtc->mode);
        drmModeFreeCrtc(crtc);

        //free all buffers
        for (std::map<uint32_t, uint32_t>::iterator iter = fbCache.begin(); iter != fbCache.end(); iter++) {
            drmModeRmFB(driDevice, iter->second);
        }

        fbCache.clear();

        if (previous_bo) {
            //release the locked front buffer
            gbm_surface_release_buffer(gbmSurface, previous_bo);
            previous_bo = NULL;
        }
#endif

        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
            context = EGL_NO_CONTEXT;
        }

        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(display, surface);
            surface = EGL_NO_SURFACE;
        }

#ifdef EGL_GBM
        if (gbmSurface) {
            gbm_surface_destroy(gbmSurface);
            gbmSurface = NULL;
        }
#endif

        eglTerminate(display);
        display = EGL_NO_DISPLAY;

#ifdef EGL_GBM
        if (displayType) {
            gbm_device_destroy((gbm_device*)displayType);
            displayType = EGL_DEFAULT_DISPLAY;
        }

        if (driDevice) {
            close(driDevice);
            driDevice = 0;
        }
#endif
    }

    removeInstance();

    if (DEBUG_GLES) {
        printf("Destroyed OpenGL ES/EGL instance. Left=%i\n", instanceCount);
    }

    if (instanceCount == 0) {
#ifdef EGL_DISPMANX
        //tvservice
        vc_tv_unregister_callback(tvservice_cb);
#endif

        //VideoCore IV
        bcm_host_deinit();

        glESInitialized = false;
    }
}

/**
 * Get default monitor resolution.
 */
bool AminoGfxRPi::getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) {
    if (DEBUG_GLES) {
        printf("getScreenInfo\n");
    }

    //default values
    w = screenW;
    h = screenH;
    refreshRate = 0; //unknown
    fullscreen = true;

#ifdef EGL_GBM
    drmModeConnector *connector = drmModeGetConnector(driDevice, connector_id);

    if (connector) {
        refreshRate = connector->modes[0].vrefresh;

        drmModeFreeConnector(connector);
    }
#endif

#ifdef EGL_DISPMANX
    TV_DISPLAY_STATE_T *tvState = getDisplayState();

    //get display properties
    if (tvState) {
        //depends on attached screen
        refreshRate = tvState->display.hdmi.frame_rate;
    }

    //free
    if (tvState) {
        free(tvState);
    }
#endif

    return true;
}

/**
 * Get runtime statistics.
 */
void AminoGfxRPi::getStats(v8::Local<v8::Object> &obj) {
    //get default values
    AminoGfx::getStats(obj);

#ifdef EGL_DISPMANX
    //HDMI (see https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h)
    //Note: works on RPi 4 too (Buster but removed in Bullseye)
    TV_DISPLAY_STATE_T *tvState = getDisplayState();

    if (!tvState) {
        //Note: does not occur on detached cable (see getDisplayState())

        return;
    }

    v8::Local<v8::Object> hdmiObj = Nan::New<v8::Object>();

    Nan::Set(obj, Nan::New("hdmi").ToLocalChecked(), hdmiObj);
//cbxx add similar
    // HDMI_DISPLAY_STATE_T
    Nan::Set(hdmiObj, Nan::New("state").ToLocalChecked(), Nan::New(tvState->display.hdmi.state));
    Nan::Set(hdmiObj, Nan::New("width").ToLocalChecked(), Nan::New(tvState->display.hdmi.width));
    Nan::Set(hdmiObj, Nan::New("height").ToLocalChecked(), Nan::New(tvState->display.hdmi.height));
    Nan::Set(hdmiObj, Nan::New("frameRate").ToLocalChecked(), Nan::New(tvState->display.hdmi.frame_rate));
    Nan::Set(hdmiObj, Nan::New("scanMode").ToLocalChecked(), Nan::New(tvState->display.hdmi.scan_mode));
    Nan::Set(hdmiObj, Nan::New("group").ToLocalChecked(), Nan::New(tvState->display.hdmi.group));
    Nan::Set(hdmiObj, Nan::New("mode").ToLocalChecked(), Nan::New(tvState->display.hdmi.mode));
    Nan::Set(hdmiObj, Nan::New("pixelRep").ToLocalChecked(), Nan::New(tvState->display.hdmi.pixel_rep));
    Nan::Set(hdmiObj, Nan::New("aspectRatio").ToLocalChecked(), Nan::New(tvState->display.hdmi.aspect_ratio));
    Nan::Set(hdmiObj, Nan::New("pixelEncoding").ToLocalChecked(), Nan::New(tvState->display.hdmi.pixel_encoding));
    Nan::Set(hdmiObj, Nan::New("format3d").ToLocalChecked(), Nan::New(tvState->display.hdmi.format_3d));
//cbxx add similar
    //display options (HDMI_DISPLAY_OPTIONS_T)
    v8::Local<v8::Object> displayObj = Nan::New<v8::Object>();

    Nan::Set(hdmiObj, Nan::New("displayOptions").ToLocalChecked(), displayObj);

    Nan::Set(displayObj, Nan::New("aspect").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.aspect));
    Nan::Set(displayObj, Nan::New("verticalBarPresent").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.vertical_bar_present));
    Nan::Set(displayObj, Nan::New("leftBarWidth").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.left_bar_width));
    Nan::Set(displayObj, Nan::New("rightBarWidth").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.right_bar_width));
    Nan::Set(displayObj, Nan::New("horizontalBarPresent").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.horizontal_bar_present));
    Nan::Set(displayObj, Nan::New("topBarHeight").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.top_bar_height));
    Nan::Set(displayObj, Nan::New("bottomBarHeight").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.bottom_bar_height));
    Nan::Set(displayObj, Nan::New("overscanFlags").ToLocalChecked(), Nan::New(tvState->display.hdmi.display_options.overscan_flags));

    //device
    TV_DEVICE_ID_T id;

    memset(&id, 0, sizeof(id));

    if (vc_tv_get_device_id(&id) == 0 && id.vendor[0] != '\0' && id.monitor_name[0] != '\0') {
        v8::Local<v8::Object> deviceObj = Nan::New<v8::Object>();

        //add monitor property
        Nan::Set(hdmiObj, Nan::New("device").ToLocalChecked(), deviceObj);

        //properties
        Nan::Set(deviceObj, Nan::New("vendor").ToLocalChecked(), Nan::New(id.vendor).ToLocalChecked());
        Nan::Set(deviceObj, Nan::New("monitorName").ToLocalChecked(), Nan::New(id.monitor_name).ToLocalChecked());
        Nan::Set(deviceObj, Nan::New("serialNum").ToLocalChecked(), Nan::New(id.serial_num));
    }
#endif

#ifdef EGL_GBM
    getDrmStats(obj);
#endif
}

#ifdef EGL_GBM
/**
 * @brief Get stata from DRM.
 *
 * @param obj
 */
void AminoGfxRPi::getDrmStats(v8::Local<v8::Object> &obj) {
    //current connector
    drmModeConnector *conn = drmModeGetConnector(driDevice, connector_id);

    if (!conn) {
        return;
    }

    //connector data
    //cbxx TODO
    std::string type = getDrmConnectorType(conn);
    std::string connected = conn->connection == DRM_MODE_CONNECTED ? "connected":"disconnected";

    //debug cbxx
    printf(" -> %s-%d (%s)\n", type.c_str(), conn->connector_type_id, connected.c_str());

    //active mode
    //cbxx TODO mode_info

    //modes
    for (int i = 0; i < conn->count_modes; i++) {
        drmModeModeInfo *mode = &conn->modes[i];

        //debug cbxx
        printf("  -> %ix%i@%i (%s)\n", mode->hdisplay, mode->vdisplay, mode->vrefresh, mode->name);
    }

    //properties (see https://github.com/ascent12/drm_info/blob/master/json.c#L376)
    drmModeObjectProperties *props = drmModeObjectGetProperties(driDevice, conn->connector_id, DRM_MODE_OBJECT_CONNECTOR);

    if (props) {
        for (uint32_t i = 0; i < props->count_props; ++i) {
            drmModePropertyRes *prop = drmModeGetProperty(driDevice, props->props[i]);

            if (!prop) {
                continue;
            }

            uint32_t flags = prop->flags;
            uint32_t type = flags & (DRM_MODE_PROP_LEGACY_TYPE | DRM_MODE_PROP_EXTENDED_TYPE);
            bool atomic = flags & DRM_MODE_PROP_ATOMIC;
            bool immutable = flags & DRM_MODE_PROP_IMMUTABLE;
            uint64_t value = props->prop_values[i];

            //debug cbxx
            printf(" -> property %s (values=%i)\n", prop->name, prop->count_values);

            //check type
            switch (type) {
                case DRM_MODE_PROP_RANGE: {
                    uint64_t rangeMin = prop->values[0];
                    uint64_t rangeMax = prop->values[1];

                    //debug cbxx
                    printf(" -> range %" PRIu64 "..%" PRIu64 ": %" PRIu64 "\n", rangeMin, rangeMax, value);
                    break;
                }

                case DRM_MODE_PROP_ENUM:
                case DRM_MODE_PROP_BITMASK:
                    //debug cbxx
                    for (int j = 0; j < prop->count_enums; ++j) {
                        printf(" -> enum %s: %" PRIu64 "\n", prop->enums[j].name, prop->enums[j].value);
                    }

                    printf(" -> value %" PRIu64 "\n", value);
                    break;

                case DRM_MODE_PROP_OBJECT:
                    //debug cbxx
                    printf(" -> object %" PRIu64 "\n", value);
                    break;

                case DRM_MODE_PROP_SIGNED_RANGE: {
                    int64_t rangeMin = (int64_t)prop->values[0];
                    int64_t rangeMax = (int64_t)prop->values[1];

                    //debug cbxx
                    printf(" -> range %" PRId64 "..%" PRId64 ": %" PRId64 "\n", rangeMin, rangeMax, (int64_t)value);
                    break;
                }

                case DRM_MODE_PROP_BLOB:
                    //debug cbxx
                    printf(" -> blob: count=%i\n %" PRIu64 "\n", prop->count_blobs, prop->values[0]);

                    //debug cbxx TODO meaning
                    for (int j = 0; j < prop->count_blobs; j++) {
                        showPropertyBlob(prop->blob_ids[j]);
                    }

                    if (value) {
                        showPropertyBlob(value);
                    }
                    break;

                default:
                    //debug cbxx
                    printf(" -> unknown %d\n", type);
                    break;
            }

            drmModeFreeProperty(prop);
        }

        drmModeFreeObjectProperties(props);
    }

    //done
    drmModeFreeConnector(conn);
}

/**
 * @brief Display a blob on console.
 *
 * See https://github.com/CPFL/drm/blob/master/tests/proptest/proptest.c#L87.
 *
 * @param id
 */
void AminoGfxRPi::showPropertyBlob(uint32_t id) {
    drmModePropertyBlobPtr blob = drmModeGetPropertyBlob(driDevice, id);

    if (!blob) {
        return;
    }

    //debug cbxx
    printf(" -> blob len=%i\n", blob->length);

    unsigned char *blob_data = (unsigned char *)blob->data;

    for (uint32_t i = 0; i < blob->length; i++) {
        if (i % 16 == 0) {
            printf("\n\t\t\t");
        }

        printf("%.2hhx", blob_data[i]);
    }

    printf("\n");

    drmModeFreePropertyBlob(blob);
}
#endif

#ifdef EGL_DISPMANX
/**
 * Switch to HDMI mode.
 *
 * Note: works on RPi 4 too but conflicts with DRM mode selection.
 */
void AminoGfxRPi::forceHdmiMode(uint32_t code) {
    if (DEBUG_HDMI) {
        printf("Changing resolution to CEA code %i\n", (int)code);
    }

    //Note: mode change takes some time (is asynchronous)
    //      see https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h
    sem_init(&resSem, 0, 0);
    resSemValid = true;

    if (vc_tv_hdmi_power_on_explicit(HDMI_MODE_HDMI, HDMI_RES_GROUP_CEA, code) != 0) {
        printf("-> failed\n");
        return;
    }

    //wait for change to occur before Dispmanx is initialized
    sem_wait(&resSem);
    sem_destroy(&resSem);
    resSemValid = false;
}

/**
 * Switch HDMI off.
 */
void AminoGfxRPi::switchHdmiOff() {
    if (DEBUG_HDMI) {
        printf("Switching HDMI off.\n");
    }

    vc_tv_power_off();
}
#endif

/**
 * Add VideoCore IV and generic properties.
 */
void AminoGfxRPi::populateRuntimeProperties(v8::Local<v8::Object> &obj) {
    if (DEBUG_GLES) {
        printf("populateRuntimeProperties()\n");
    }

    AminoGfx::populateRuntimeProperties(obj);

    //GLES
    Nan::Set(obj, Nan::New("eglVendor").ToLocalChecked(), Nan::New(std::string(eglQueryString(display, EGL_VENDOR))).ToLocalChecked());
    Nan::Set(obj, Nan::New("eglVersion").ToLocalChecked(), Nan::New(std::string(eglQueryString(display, EGL_VERSION))).ToLocalChecked());

    //DRM
#ifdef EGL_GBM
    drmVersionPtr version = drmGetVersion(driDevice);
    std::ostringstream ss;

    //Note: major and minor not set!
    ss << version->version_major << "." << version->version_minor << " (" << version-> name << ", " << version->date << ", " << version->desc << ")";

    Nan::Set(obj, Nan::New("drm").ToLocalChecked(), Nan::New(ss.str()).ToLocalChecked());

    drmFreeVersion(version);
#endif

#ifdef EGL_DISPMANX
    //VC (Note: works on RPi 4 too, missing on Bullseye)
    char resp[80] = "";

    //Note: does not work on RPi 4!
    if (vc_gencmd(resp, sizeof resp, "get_mem gpu") == 0) {
        //GPU memory in MB
        int gpuMem = 0;

        vc_gencmd_number_property(resp, "gpu", &gpuMem);

        if (gpuMem > 0) {
            Nan::Set(obj, Nan::New("gpu_mem").ToLocalChecked(), Nan::New(gpuMem));

            //debug
            //printf("gpu_mem: %i\n", gpuMem);
        }
    }
#endif

    //TODO support on Bullseye

    //build info
#ifdef RPI_BUILD
    Nan::Set(obj, Nan::New("build").ToLocalChecked(), Nan::New(RPI_BUILD).ToLocalChecked());
#endif
}

/**
 * Initialize OpenGL ES.
 */
void AminoGfxRPi::initRenderer() {
    if (DEBUG_GLES) {
        printf("initRenderer()\n");
    }

    //base
    AminoGfx::initRenderer();

    //set display size & viewport
    updateSize(screenW, screenH);
    updatePosition(0, 0);

    viewportW = screenW;
    viewportH = screenH;
    viewportChanged = true;

#ifdef EGL_DISPMANX
    if (DEBUG_HDMI) {
        printf("-> init Dispmanx\n");
    }

    //Dispmanx
    surface = createDispmanxSurface();
#endif

#ifdef EGL_GBM
    if (DEBUG_HDMI) {
        printf("-> init GBM\n");
    }

    //GBM
    surface = createGbmSurface();
#endif

    //activate context (needed by JS code to create shaders)
    EGLBoolean res = eglMakeCurrent(display, surface, surface, context);

    assert(EGL_FALSE != res);

    //swap interval
    if (swapInterval != 0) {
        res = eglSwapInterval(display, swapInterval);

        assert(res == EGL_TRUE);
    }

    //input
    initInput();
}

#ifdef EGL_DISPMANX
/**
 * Get EGL surface from Dispmanx (RPi 3 and lower).
 */
EGLSurface AminoGfxRPi::createDispmanxSurface() {
    //Dispmanx init
    DISPMANX_DISPLAY_HANDLE_T dispman_display = vc_dispmanx_display_open(0); //LCD
    DISPMANX_UPDATE_HANDLE_T dispman_update = vc_dispmanx_update_start(0);

    int LAYER = 0;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;

    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = screenW;
    dst_rect.height = screenH;

    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = screenW << 16;
    src_rect.height = screenH << 16;

    VC_DISPMANX_ALPHA_T dispman_alpha;

    //Notes: works but seeing issues with font shader which uses transparent pixels! The background is visible at the border pixels of the font!
    /*
    dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FROM_SOURCE;
    dispman_alpha.opacity = 255;
    dispman_alpha.mask = 0;
    */

    dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
    dispman_alpha.opacity = 0xFF;
    dispman_alpha.mask = 0;

    DISPMANX_ELEMENT_HANDLE_T dispman_element = vc_dispmanx_element_add(
        dispman_update,
        dispman_display,
        LAYER, //layer
        &dst_rect,
        0, //src
        &src_rect,
        DISPMANX_PROTECTION_NONE,
        &dispman_alpha, //alpha
        0, //clamp,
        (DISPMANX_TRANSFORM_T)0 //transform
    );

    vc_dispmanx_update_submit_sync(dispman_update);

    //create EGL surface
    static EGL_DISPMANX_WINDOW_T native_window;

    native_window.element = dispman_element;
    native_window.width = screenW;
    native_window.height = screenH;

    EGLSurface surface = eglCreateWindowSurface(display, config, &native_window, NULL);

    //Note: happens for instance if there is a resource leak (restart the RPi in this case)
    assert(surface != EGL_NO_SURFACE);

    return surface;
}
#endif

#ifdef EGL_GBM
/**
 * Get EGL surface from GBM.
 */
EGLSurface AminoGfxRPi::createGbmSurface() {
    //create surface
    gbmSurface = gbm_surface_create((gbm_device*)displayType, mode_info.hdisplay, mode_info.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

    assert(gbmSurface);

    EGLSurface surface = eglCreateWindowSurface(display, config, gbmSurface, NULL);

    assert(surface != EGL_NO_SURFACE);

    return surface;
}

/**
 * Get the DRM connector type.
 */
std::string AminoGfxRPi::getDrmConnectorType(drmModeConnector *connector) {
    switch (connector->connector_type) {
        case DRM_MODE_CONNECTOR_VGA:
            return "VGA";

        case DRM_MODE_CONNECTOR_DVII:
            return "DVI-I";

        case DRM_MODE_CONNECTOR_DVID:
            return "DVI-D";

        case DRM_MODE_CONNECTOR_DVIA:
            return "DVI-A";

        case DRM_MODE_CONNECTOR_Composite:
            return "Composite";

        case DRM_MODE_CONNECTOR_SVIDEO:
            return "SVIDEO";

        case DRM_MODE_CONNECTOR_LVDS:
            return "LVDS";

        case DRM_MODE_CONNECTOR_Component:
            return "Component";

        case DRM_MODE_CONNECTOR_9PinDIN:
            return "DIN";

        case DRM_MODE_CONNECTOR_DisplayPort:
            return "DisplayPort";

        case DRM_MODE_CONNECTOR_HDMIA:
            return "HDMI-A";

        case DRM_MODE_CONNECTOR_HDMIB:
            return "HDMI-B";

        case DRM_MODE_CONNECTOR_TV:
            return "TV";

        case DRM_MODE_CONNECTOR_eDP:
            return "eDP";

        case DRM_MODE_CONNECTOR_VIRTUAL:
            return "Virtual";

        case DRM_MODE_CONNECTOR_DSI:
            return "DSI";

        case DRM_MODE_CONNECTOR_DPI:
            return "DPI";

        case DRM_MODE_CONNECTOR_Unknown:
        default:
            return "unknown";
    }
}

#endif

bool AminoGfxRPi::startsWith(const char *pre, const char *str) {
    size_t lenpre = strlen(pre);
    size_t lenstr = strlen(str);

    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

const char* INPUT_DIR = "/dev/input";

void AminoGfxRPi::initInput() {
    if ((getuid()) != 0) {
        printf("You are not root. This might not work\n");
    }

    //check files in input folder
    DIR *dir;
    struct dirent *file;

    dir = opendir(INPUT_DIR);

    if (dir) {
        while ((file = readdir(dir)) != NULL) {
            if (!startsWith("event", file->d_name)) {
                //debug
                //printf("Ignored %s\n", file->d_name);

                continue;
            }

            if (DEBUG_INPUT) {
                printf("File = %s\n", file->d_name);
                printf("Initializing the device.\n");
            }

            //create device
            std::string filename = std::string(INPUT_DIR) + "/" + std::string(file->d_name);
            AminoInputRPi *inputDevice = new AminoInputRPi(this, filename);

            if (!inputDevice->init()) {
                delete inputDevice;
                continue;
            }

            inputDevices.push_back(inputDevice);
        }

        closedir(dir);
    }
}

void AminoGfxRPi::start() {
    if (DEBUG_GLES) {
        printf("start()\n");
    }

    //ready to get control back to JS code
    ready();

    //detach context from main thread
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

bool AminoGfxRPi::bindContext() {
    //bind OpenGL context
    if (surface == EGL_NO_SURFACE) {
        return false;
    }

    EGLBoolean res = eglMakeCurrent(display, surface, surface, context);

    assert(res == EGL_TRUE);

    return true;
}

void AminoGfxRPi::renderingDone() {
    if (DEBUG_GLES) {
        printf("renderingDone()\n");
    }

    //swap buffer
    EGLBoolean res = eglSwapBuffers(display, surface);

    assert(res == EGL_TRUE);

#ifdef EGL_GBM
    //lock current front buffer and return a new bo
    struct gbm_bo *bo = gbm_surface_lock_front_buffer(gbmSurface);

    assert(bo);

    uint32_t handle = gbm_bo_get_handle(bo).u32;

    //debug
    if (DEBUG_GLES) {
        printf("-> bo handle: %i\n", handle);
    }

    //cache framebuffers
    std::map<uint32_t, uint32_t>::iterator it = fbCache.find(handle);
    uint32_t fb;

    if (it != fbCache.end()) {
        //use cached fb
        fb = it->second;
    } else {
        //create new fb (see https://docs.nvidia.com/drive/nvvib_docs/NVIDIA%20DRIVE%20Linux%20SDK%20Development%20Guide/baggage/group__direct__rendering__manager.html)
        uint32_t pitch = gbm_bo_get_stride(bo);

        //drmModeAddFB() version
        /*
        uint8_t depth = 24;
        uint8_t bpp = 32;

        int res = drmModeAddFB(driDevice, mode_info.hdisplay, mode_info.vdisplay, depth, bpp, pitch, handle, &fb);
        */

        //drmModeAddFB2() version
        uint32_t format = gbm_bo_get_format(bo); //GBM_FORMAT_XRGB8888
        uint32_t handles[4] = { handle, 0, 0, 0 };
        uint32_t pitches[4] = { pitch, 0, 0, 0 };
        uint32_t offsets[4] = { 0, 0, 0, 0 };
        uint32_t plane_flags = 0;

        if (DEBUG_GLES) {
            switch (format) {
                case GBM_FORMAT_XRGB8888:
                    printf("-> bo format: XRGB8888\n");
                    break;

                case GBM_FORMAT_ARGB8888:
                    printf("-> bo format: ARGB8888\n");
                    break;

                default:
                    printf("-> bo format: unknown %d\n", format);
                    break;
            }
        }

        int res = drmModeAddFB2(driDevice, mode_info.hdisplay, mode_info.vdisplay, format, handles, pitches, offsets, &fb, plane_flags);

        assert(res == 0);
        assert(fb);

        fbCache.insert(std::pair<uint32_t, uint32_t>(handle, fb));

        //debug
        if (DEBUG_GLES) {
            printf("-> created fb\n");
        }

        if (USE_DRM_PAGEFLIP) {
            //set CRTC once
            int res2 = drmModeSetCrtc(driDevice, crtc->crtc_id, fb, 0, 0, &connector_id, 1, &mode_info);

            assert(res2 == 0);
        }
    }

    /*
     * Page flip issues:
     *
     *   - layers.js
     *     - reduces framerate to 30 fps with just a few layers
     *   - opacity.js
     *     - seeing tearing
     *   - video playback
     *     - seeing tearing
     *
     *  Without page flipping:
     *
     *   - tearing looks equal
     *   - getting more frames being rendered
     *     - not sure -> still seeing 30 or 60 fps
     *     - depends on screen???
     *
     *  => not using page flipping right now
     *
     */

    if (USE_DRM_PAGEFLIP) {
        //signal page flip (see https://raw.githubusercontent.com/dvdhrm/docs/master/drm-howto/modeset-vsync.c)
        int res2 = drmModePageFlip(driDevice, crtc->crtc_id, fb, DRM_MODE_PAGE_FLIP_EVENT, this);

        //debug
        //printf("-> page flip res: %d (EINVAL=%d EBUSY=%d)\n", res2, EINVAL, EBUSY);

        assert(res2 == 0);

        if (res2 == 0) {
            pageFlipPending = true;
        }

        //wait for page flip
        fd_set fds;
        drmEventContext ev;

        memset(&ev, 0, sizeof(ev));
        ev.version = DRM_EVENT_CONTEXT_VERSION;
        ev.page_flip_handler = handlePageFlipEvent;

        while (pageFlipPending) {
            //cbxx TODO check
            //TODO needed? Not handling any input here.
            FD_ZERO(&fds);
            FD_SET(0, &fds);
            FD_SET(driDevice, &fds);

            int ret = select(driDevice + 1, &fds, NULL, NULL, NULL);

            assert(ret);

            //debug
            /*
            if (ret < 0) {
                printf("select err: %s\n", strerror(errno));
            } else if (ret == 0) {
                printf("select timeout!\n");
            } else if (FD_ISSET(0, &fds)) {
                printf("user interrupted!\n");
            }
            */

            //handle events
            ret = drmHandleEvent(driDevice, &ev);

            assert(ret == 0);

            //debug
            /*
            if (ret) {
                printf("-> drmHandleEvent() failed %d\n", ret);

                break;
            }
            */
        }
    } else {
        //double buffering case without vsync (two framebuffers)

        int res2 = drmModeSetCrtc(driDevice, crtc->crtc_id, fb, 0, 0, &connector_id, 1, &mode_info);

        assert(res2 == 0);
    }

    //free previous
    if (previous_bo) {
        //release old bo
        gbm_surface_release_buffer(gbmSurface, previous_bo);

        //Note: previous_bo value set below
    }

    //prepare next
    previous_bo = bo;
#endif

    if (DEBUG_GLES) {
        printf("-> EGL buffers swapped\n");
    }
}

#ifdef EGL_GBM
/**
 * Handle a page flip event.
 */
void AminoGfxRPi::handlePageFlipEvent(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
    //debug
    //printf("-> page flip occured\n");

    static_cast<AminoGfxRPi *>(data)->pageFlipPending = false;
}
#endif

void AminoGfxRPi::handleSystemEvents() {
    //handle events
    processInputs();
}

void AminoGfxRPi::processInputs() {
    if (DEBUG_GLES) {
        printf("processInputs()\n");
    }

    //poll all input devices
    for (const auto& inputDevice : inputDevices) {
        inputDevice->process();
    }

    if (DEBUG_GLES) {
        printf("-> done\n");
    }
}

/**
 * Update the window size.
 *
 * Note: has to be called on main thread
 */
void AminoGfxRPi::updateWindowSize() {
    //not supported

    //reset to screen values
    propW->setValue(screenW);
    propH->setValue(screenH);
}

/**
 * Update the window position.
 *
 * Note: has to be called on main thread
 */
void AminoGfxRPi::updateWindowPosition() {
    //not supported
    propX->setValue(0);
    propY->setValue(0);
}

/**
 * Update the title.
 *
 * Note: has to be called on main thread
 */
void AminoGfxRPi::updateWindowTitle() {
    //not supported
}

/**
 * Shared atlas texture has changed.
 */
void AminoGfxRPi::atlasTextureHasChanged(texture_atlas_t *atlas) {
    //check single instance case
    if (instanceCount == 1) {
        return;
    }

    //run on main thread
    enqueueJSCallbackUpdate(static_cast<jsUpdateCallback>(&AminoGfxRPi::atlasTextureHasChangedHandler), NULL, atlas);
}

/**
 * Handle on main thread.
 */
void AminoGfxRPi::atlasTextureHasChangedHandler(JSCallbackUpdate *update) {
    AminoGfx *gfx = static_cast<AminoGfx *>(update->obj);
    texture_atlas_t *atlas = (texture_atlas_t *)update->data;

    for (auto const &item : instances) {
        if (gfx == item) {
            continue;
        }

        static_cast<AminoGfxRPi *>(item)->updateAtlasTexture(atlas);
    }
}

/**
 * Create video player.
 */
AminoVideoPlayer* AminoGfxRPi::createVideoPlayer(AminoTexture *texture, AminoVideo *video) {
    return new AminoOmxVideoPlayer(texture, video);
}

/**
 * Create EGL Image.
 */
EGLImageKHR AminoGfxRPi::createEGLImage(GLuint textureId) {
    /*
     * Notes:
     *
     * - In case of failure check gpu_mem is high enough and only one process is using OMX.
     *
     * Sample error:
     *
     *   eglCreateImageKHR:  failed to create image for buffer 0x4 target 12465 error 0x300c
     */

    return eglCreateImageKHR(display, context, EGL_GL_TEXTURE_2D_KHR, (EGLClientBuffer)textureId, 0);
}

/**
 * Destroy EGL Image.
 */
void AminoGfxRPi::destroyEGLImage(EGLImageKHR eglImage) {
    //switch to rendering thread
    AminoJSObject::enqueueValueUpdate(0, eglImage, static_cast<asyncValueCallback>(&AminoGfxRPi::destroyEGLImageHandler));
}

/**
 * Destroy EGL Image texture on OpenGL thread.
 */
void AminoGfxRPi::destroyEGLImageHandler(AsyncValueUpdate *update, int state) {
    if (state != AsyncValueUpdate::STATE_APPLY) {
        return;
    }

    assert(update->data);

    if (DEBUG_VIDEOS) {
        printf("destroying EGL image\n");
    }

    EGLBoolean res = eglDestroyImageKHR(display, (EGLImageKHR)update->data);

    assert(res == EGL_TRUE);
}

//static initializers
bool AminoGfxRPi::glESInitialized = false;

#ifdef EGL_DISPMANX
sem_t AminoGfxRPi::resSem;
bool AminoGfxRPi::resSemValid = false;
#endif
//
// AminoGfxRPiFactory
//

/**
 * Create AminoGfx factory.
 */
AminoGfxRPiFactory::AminoGfxRPiFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoGfx", callback) {
    //empty
}

/**
 * Create AminoGfx instance.
 */
AminoJSObject* AminoGfxRPiFactory::create() {
    return new AminoGfxRPi();
}

void crashHandler(int sig) {
    void *array[10];
    size_t size;

    //process & thread
    pid_t pid = getpid();
    pid_t tid = gettid();
    uv_thread_t threadId = uv_thread_self();

    //get void*'s for all entries on the stack
    size = backtrace(array, 10);

    //print out all the frames to stderr
    fprintf(stderr, "Error: signal %d (process=%d, thread=%d, uvThread=%lu):\n", sig, pid, tid, (unsigned long)threadId);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// ========== Event Callbacks ===========

NAN_MODULE_INIT(InitAll) {
    //crash handler
    signal(SIGSEGV, crashHandler);

    //main class
    AminoGfxRPi::Init(target);

    //amino classes
    AminoGfx::InitClasses(target);
}

//entry point
NODE_MODULE(aminonative, InitAll)
