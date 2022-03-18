#include "rpi.h"

#include <dirent.h>
#include <stdio.h>

#include <execinfo.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include <sstream>

#define gettid() syscall(SYS_gettid)

//debug cbxx
#define DEBUG_GLES false
#define DEBUG_RENDER false
#define DEBUG_INPUT true
#define DEBUG_HDMI false

#define USE_DRM_PAGEFLIP false

#define AMINO_EGL_SAMPLES 4
#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))

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

        //VideoCore IV
        bcm_host_init();

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
    AminoGfx::getStats(obj);

    //HDMI (see https://github.com/raspberrypi/userland/blob/master/interface/vmcs_host/vc_hdmi.h)
    //Note: works on RPi 4 too
    TV_DISPLAY_STATE_T *tvState = getDisplayState();

    if (!tvState) {
        //Note: does not occur on detached cable (see getDisplayState())

        return;
    }

    v8::Local<v8::Object> hdmiObj = Nan::New<v8::Object>();

    Nan::Set(obj, Nan::New("hdmi").ToLocalChecked(), hdmiObj);

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
}

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
 * Add VideoCore IV properties.
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

    //VC (Note: works on RPi 4 too)
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
                //debug cbxx
                printf("Ignored %s\n", file->d_name);

                continue;
            }

            //cbxx TODO verify
            if (DEBUG_INPUT) {
                printf("File = %s\n", file->d_name);
                printf("Initializing the device.\n");
            }

            //open device
            char str[256];

            strcpy(str, INPUT_DIR);
            strcat(str, "/");
            strcat(str, file->d_name);

            int fd = -1;

            if ((fd = open(str, O_RDONLY | O_NONBLOCK)) == -1) {
                printf("This is not a valid device %s\n", str);
                continue;
            }

            //get device name
            char name[256] = "Unknown";

            ioctl(fd, EVIOCGNAME(sizeof name), name);
            printf("Reading from: %s (%s)\n", str, name);

            //get device location
            ioctl(fd, EVIOCGPHYS(sizeof name), name);
            printf("Location %s (%s)\n", str, name);

            //get device info
            struct input_id device_info;

            ioctl(fd, EVIOCGID, &device_info);

            //iterate event types
            u_int8_t evtype_b[(EV_MAX + 7) / 8];

            memset(evtype_b, 0, sizeof evtype_b);

            if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtype_b) < 0) {
                printf("Error reading device info.\n");
                continue;
            }

            for (int i = 0; i < EV_MAX; i++) {
                if (test_bit(i, evtype_b)) {
                    //see https://www.kernel.org/doc/Documentation/input/event-codes.txt
                    printf("Event type 0x%02x ", i);

                    switch (i) {
                        case EV_SYN:
                            printf("sync events\n");
                            break;

                        case EV_KEY:
                            //keyboard, buttons, ...
                            printf("key events\n");
                            break;

                        case EV_REL:
                            printf("rel events\n");
                            break;

                        case EV_ABS:
                            //touchscreen
                            printf("abs events\n");

                            initTouch()
                            break;

                        case EV_MSC:
                            //other input data
                            printf("msc events\n");
                            break;

                        case EV_SW:
                            printf("sw events\n");
                            break;

                        case EV_LED:
                            printf("led events\n");
                            break;

                        case EV_SND:
                            printf("snd events\n");
                            break;

                        case EV_REP:
                            printf("rep events\n");
                            break;

                        case EV_FF:
                            printf("ff events\n");
                            break;

                        default:
                            printf("unknown\n");
                            break;
                    }
                }
            }

            fds.push_back(fd);
        }

        closedir(dir);
    }
}

void AminoGfxRPi::initTouch(int fd) {
    //get details
    int abs[6] = { 0 };
    unsigned long bit[NBITS(KEY_MAX)];

    ioctl(fd, EVIOCGBIT(EV_ABS, KEY_MAX), bit]);

    for (int i = 0; i < KEY_MAX; i++) {
        if (!test_bit(i, bit)) {
            continue;
        }

        printf("    Event code %d\n", i);

        ioctl(fd, EVIOCGABS(i), abs);

        for (int j = 0; j < 5; j++) {
            if (j < 3 || abs[j]) {
                printf("     %s %6d\n", absval[j], abs[j]);

                if (i == 0) {
                    if (absval[j] == "Min  ")  {
                        touch_x_min = abs[j];
                    }

                    if (absval[j] == "Max  ") {
                        touch_x_max = abs[j];
                    }
                }

                if (i == 1) {
                    if (absval[j] == "Min  ") {
                        touch_y_min = abs[j];
                    }

                    if (absval[j] == "Max  ") {
                        touch_y_max = abs[j];
                    }
                }
            }
        }
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
    int size = sizeof(struct input_event);
    struct input_event ev[64];

    for (unsigned int i = 0; i < fds.size(); i++) {
        int fd = fds[i];
        int rd = read(fd, ev, size*64);

        if (rd == -1) {
            continue;
        }

        if (rd < size) {
            printf("Read too little!!!  %d\n", rd);
            continue;
        }

        for (int i = 0; i < (int)(rd / size); i++) {
            if (DEBUG_INPUT) {
                dump_event(&(ev[i]));
            }

            handleEvent(ev[i]);
        }
    }

    if (DEBUG_GLES) {
        printf("-> done\n");
    }
}

//cbxx TODO TODO touchscreen
void AminoGfxRPi::handleEvent(input_event ev) {
    //relative event. probably mouse
    if (ev.type == EV_REL) {
        if (ev.code == 0) {
            //x axis
            mouse_x += ev.value;
        }

        if (ev.code == 1) {
            mouse_y += ev.value;
        }

        if (mouse_x < 0) {
            mouse_x = 0;
        }

        if (mouse_y < 0) {
            mouse_y = 0;
        }

        if (mouse_x >= (int)screenW)  {
            mouse_x = (int)screenW - 1;
        }

        if (mouse_y >= (int)screenH) {
            mouse_y = (int)screenH - 1;
        }

        //TODO GLFW_MOUSE_POS_CALLBACK_FUNCTION(mouse_x, mouse_y);

        return;
    }

    //mouse wheel
    if (ev.type == EV_REL && ev.code == 8) {
        //TODO GLFW_MOUSE_WHEEL_CALLBACK_FUNCTION(ev.value);
        return;
    }

    if (ev.type == EV_KEY) {
        if (DEBUG_GLES || DEBUG_INPUT) {
            printf("Key or button pressed code = %d, state = %d\n", ev.code, ev.value);
        }

        if (ev.code == BTN_LEFT) {
            //TODO GLFW_MOUSE_BUTTON_CALLBACK_FUNCTION(ev.code, ev.value);
            return;
        } else {
            //create object
            v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

            if (!ev.value) {
                //release
                Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("key.release").ToLocalChecked());
            } else if (ev.value) {
                //press or repeat
                Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("key.press").ToLocalChecked());
            }

            //key codes
            int keycode = -1;

            if (ev.code >= KEY_1 && ev.code <= KEY_9) {
                keycode = ev.code - KEY_1 + 49;
            }

            switch (ev.code) {
                // from https://www.glfw.org/docs/latest/group__keys.html

                case KEY_0: keycode = 48; break;
                case KEY_1: keycode = 49; break;
                case KEY_2: keycode = 50; break;
                case KEY_3: keycode = 51; break;
                case KEY_4: keycode = 52; break;
                case KEY_5: keycode = 53; break;
                case KEY_6: keycode = 54; break;
                case KEY_7: keycode = 55; break;
                case KEY_8: keycode = 56; break;
                case KEY_9: keycode = 57; break;
                case KEY_SEMICOLON: keycode = 59 /* ; */; break;
                case KEY_EQUAL: keycode = 61 /* = */; break;
                case KEY_A: keycode = 65; break;
                case KEY_B: keycode = 66; break;
                case KEY_C: keycode = 67; break;
                case KEY_D: keycode = 68; break;
                case KEY_E: keycode = 69; break;
                case KEY_F: keycode = 70; break;
                case KEY_G: keycode = 71; break;
                case KEY_H: keycode = 72; break;
                case KEY_I: keycode = 73; break;
                case KEY_J: keycode = 74; break;
                case KEY_K: keycode = 75; break;
                case KEY_L: keycode = 76; break;
                case KEY_M: keycode = 77; break;
                case KEY_N: keycode = 78; break;
                case KEY_O: keycode = 79; break;
                case KEY_P: keycode = 80; break;
                case KEY_Q: keycode = 81; break;
                case KEY_R: keycode = 82; break;
                case KEY_S: keycode = 83; break;
                case KEY_T: keycode = 84; break;
                case KEY_U: keycode = 85; break;
                case KEY_V: keycode = 86; break;
                case KEY_W: keycode = 87; break;
                case KEY_X: keycode = 88; break;
                case KEY_Y: keycode = 89; break;
                case KEY_Z: keycode = 90; break;
                case KEY_SPACE: keycode = 32; break;
                case KEY_APOSTROPHE: keycode = 39 /* ' */; break;
                case KEY_COMMA: keycode = 44 /* , */; break;
                case KEY_MINUS: keycode = 45 /* - */; break;
                case KEY_DOT: keycode = 46 /* . */; break;
                case KEY_SLASH: keycode = 47 /* / */; break;
                case KEY_LEFTBRACE: keycode = 91 /* [ */; break;
                case KEY_BACKSLASH: keycode = 92 /* \ */; break;
                case KEY_RIGHTBRACE: keycode = 93 /* ] */; break;
                case KEY_GRAVE: keycode = 96 /* ` */; break;
                case KEY_ESC: keycode = 256; break;
                case KEY_ENTER: keycode = 257; break;
                case KEY_TAB: keycode = 258; break;
                case KEY_BACKSPACE: keycode = 259; break;
                case KEY_INSERT: keycode = 260; break;
                case KEY_DELETE: keycode = 261; break;
                case KEY_RIGHT: keycode = 262; break;
                case KEY_LEFT: keycode = 263; break;
                case KEY_DOWN: keycode = 264; break;
                case KEY_UP: keycode = 265; break;
                case KEY_PAGEUP: keycode = 266; break;
                case KEY_PAGEDOWN: keycode = 267; break;
                case KEY_HOME: keycode = 268; break;
                case KEY_END: keycode = 269; break;
                case KEY_CAPSLOCK: keycode = 280; break;
                case KEY_SCROLLLOCK: keycode = 281; break;
                case KEY_NUMLOCK: keycode = 282; break;
                case KEY_PAUSE: keycode = 284; break;
                case KEY_F1: keycode = 290; break;
                case KEY_F2: keycode = 291; break;
                case KEY_F3: keycode = 292; break;
                case KEY_F4: keycode = 293; break;
                case KEY_F5: keycode = 294; break;
                case KEY_F6: keycode = 295; break;
                case KEY_F7: keycode = 296; break;
                case KEY_F8: keycode = 297; break;
                case KEY_F9: keycode = 298; break;
                case KEY_F10: keycode = 299; break;
                case KEY_F11: keycode = 300; break;
                case KEY_F12: keycode = 301; break;
                case KEY_F13: keycode = 302; break;
                case KEY_F14: keycode = 303; break;
                case KEY_F15: keycode = 304; break;
                case KEY_F16: keycode = 305; break;
                case KEY_F17: keycode = 306; break;
                case KEY_F18: keycode = 307; break;
                case KEY_F19: keycode = 308; break;
                case KEY_F20: keycode = 309; break;
                case KEY_F21: keycode = 310; break;
                case KEY_F22: keycode = 311; break;
                case KEY_F23: keycode = 312; break;
                case KEY_F24: keycode = 313; break;
                case KEY_KP0: keycode = 320; break;
                case KEY_KP1: keycode = 321; break;
                case KEY_KP2: keycode = 322; break;
                case KEY_KP3: keycode = 323; break;
                case KEY_KP4: keycode = 324; break;
                case KEY_KP5: keycode = 325; break;
                case KEY_KP6: keycode = 326; break;
                case KEY_KP7: keycode = 327; break;
                case KEY_KP8: keycode = 328; break;
                case KEY_KP9: keycode = 329; break;
                case KEY_KPDOT: keycode = 330; break;
                case KEY_LEFTSHIFT: keycode = 340; break;
                case KEY_LEFTCTRL: keycode = 341; break;
                case KEY_LEFTALT: keycode = 342; break;
                case KEY_RIGHTSHIFT: keycode = 344; break;
                case KEY_RIGHTCTRL: keycode = 345; break;
                case KEY_RIGHTALT: keycode = 346; break;
            }

            Nan::Set(event_obj, Nan::New("keycode").ToLocalChecked(), Nan::New(keycode));

            if (keycode == -1) {
                printf("ERROR: unknown linux key code %i\n", ev.code);
            } else {
                this->fireEvent(event_obj);
            }
        }

        //TODO GLFW_KEY_CALLBACK_FUNCTION(ev.code, ev.value);

        return;
    }
}

//cbxx check events
void AminoGfxRPi::dump_event(struct input_event *event) {
    switch(event->type) {
        case EV_SYN:
            printf("EV_SYN  event separator\n");
            break;

        case EV_KEY:
            printf("EV_KEY  keyboard or button \n");

            if (event ->code == KEY_A) {
                printf("  A key\n");
            }

            if (event ->code == KEY_B) {
                printf("  B key\n");
            }
            break;

        case EV_REL:
            printf("EV_REL  relative axis\n");
            break;

        case EV_ABS:
            printf("EV_ABS  absolute axis\n");
            break;

        case EV_MSC:
            printf("EV_MSC  misc\n");
            if (event->code == MSC_SERIAL) {
                printf("  serial\n");
            }

            if (event->code == MSC_PULSELED) {
                printf("  pulse led\n");
            }

            if (event->code == MSC_GESTURE) {
                printf("  gesture\n");
            }

            if (event->code == MSC_RAW) {
                printf("  raw\n");
            }

            if (event->code == MSC_SCAN) {
                printf("  scan\n");
            }

            if (event->code == MSC_MAX) {
                printf("  max\n");
            }
            break;

        case EV_LED:
            printf("EV_LED  led\n");
            break;

        case EV_SND:
            printf("EV_SND  sound\n");
            break;

        case EV_REP:
            printf("EV_REP  autorepeating\n");
            break;

        case EV_FF:
            printf("EV_FF   force feedback send\n");
            break;

        case EV_PWR:
            printf("EV_PWR  power button\n");
            break;

        case EV_FF_STATUS:
            printf("EV_FF_STATUS force feedback receive\n");
            break;

        case EV_MAX:
            printf("EV_MAX  max value\n");
            break;
    }

    printf("type = %d code = %d value = %d\n",event->type,event->code,event->value);
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
