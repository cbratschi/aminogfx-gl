#include "glfw.h"

#include <execinfo.h>
#include <unistd.h>
#include <pthread.h>

//cbxx FIXME crashes on M1 (e.g. move or resize window; see https://github.com/glfw/glfw/issues/1997)
#define DEBUG_GLFW false
#define DEBUG_RENDER false
#define DEBUG_VIDEO_TIMING false

/**
 * GLFW AminoGfx implementation.
 *
 */
class AminoGfxGlfw : public AminoGfx {
public:
    AminoGfxGlfw(): AminoGfx(getFactory()->name) {
        //empty
    }

    ~AminoGfxGlfw() {
        if (!destroyed) {
            destroyAminoGfxGlfw();
        }
    }

    /**
     * Get factory instance.
     */
    static AminoGfxGlfwFactory* getFactory() {
        static AminoGfxGlfwFactory *instance = NULL;

        if (!instance) {
            instance = new AminoGfxGlfwFactory(New);
        }

        return instance;
    }

    /**
     * Add class template to module exports.
     */
    static NAN_MODULE_INIT(Init) {
        AminoGfxGlfwFactory *factory = getFactory();

        AminoGfx::Init(target, factory);
    }

private:
    static bool glfwInitialized;
    static std::map<GLFWwindow *, AminoGfxGlfw *> *windowMap;

    //GLFW window
    GLFWwindow *window = NULL;

    //GLFW monitor (fullscreen start parameters)
    GLFWmonitor *monitor = NULL;
    GLFWvidmode mode = {
        .width = 0,
        .height = 0,
        .redBits = 0,
        .greenBits = 0,
        .blueBits = 0,
        .refreshRate = 0
    };
    std::string lastMonitor = "";

    static AminoGfxGlfw* windowToInstance(GLFWwindow *window) {
        std::map<GLFWwindow *, AminoGfxGlfw *>::iterator it = windowMap->find(window);

        if (it != windowMap->end()) {
            return it->second;
        }

        return NULL;
    }

    /**
     * JS object construction.
     */
    static NAN_METHOD(New) {
        AminoJSObject::createInstance(info, getFactory());
    }

    /**
     * Setup JS instance.
     */
    void setup() override {
        if (DEBUG_GLFW) {
            printf("AminoGfxGlfw.setup()\n");
        }

        //init GLFW
        if (!glfwInitialized) {
            //error handler
            glfwSetErrorCallback(handleErrors);

            if (!glfwInit()) {
                //exit on error
                printf("error. quitting\n");

                glfwTerminate();
                exit(EXIT_FAILURE);
            }

            //monitor handler
            glfwSetMonitorCallback(handleMonitorEvents);

            glfwInitialized = true;
        }

        //handle params
        if (!createParams.IsEmpty()) {
            v8::Local<v8::Object> obj = Nan::New(createParams);

            //fullscreen
            Nan::MaybeLocal<v8::Value> fullscreenMaybe = Nan::Get(obj, Nan::New<v8::String>("fullscreen").ToLocalChecked());

            if (!fullscreenMaybe.IsEmpty()) {
                v8::Local<v8::Value> fullscreenValue = fullscreenMaybe.ToLocalChecked();

                if (fullscreenValue->IsBoolean()) {
                    bool fullscreen = Nan::To<v8::Boolean>(fullscreenValue).ToLocalChecked()->Value();

                    if (fullscreen) {
                        //set monitor (default: primary)
                        monitor = glfwGetPrimaryMonitor();

                        assert(monitor);

                        //use current mode
                        mode = *getVideoMode(monitor);
                    }
                }
            }
        }

        //instance
        addInstance();

        //base class
        AminoGfx::setup();
    }

    /**
     * Destroy GLFW instance.
     */
    void destroy() override {
        if (destroyed) {
            return;
        }

        //instance
        destroyAminoGfxGlfw();

        //destroy basic instance
        AminoGfx::destroy();
    }

    /**
     * Destroy GLFW instance.
     */
    void destroyAminoGfxGlfw() {
        //GLFW
        if (window) {
            glfwDestroyWindow(window);
            windowMap->erase(window);
            window = NULL;
        }

        removeInstance();

        if (DEBUG_GLFW) {
            printf("Destroyed GLFW instance. Left=%i\n", instanceCount);
        }

        if (instanceCount == 0) {
            glfwTerminate();
            glfwInitialized = false;
        }
    }

    /**
     * Get current monitor info.
     */
    void getMonitorInfo(v8::Local<v8::Value> &value) override {
        //get monitor properties
        GLFWmonitor *monitor = this->monitor ? this->monitor:glfwGetPrimaryMonitor();

        if (!monitor) {
            return;
        }

        getMonitorInfo(monitor, value);
    }

    /**
     * @brief Get info about a monitor.
     *
     * @param monitor
     * @param value
     */
    void getMonitorInfo(GLFWmonitor *monitor, v8::Local<v8::Value> &value) {
        v8::Local<v8::Object> obj = Nan::New<v8::Object>();

        value = obj;

        //name
        Nan::Set(obj, Nan::New("name").ToLocalChecked(), Nan::New(std::string(glfwGetMonitorName(monitor))).ToLocalChecked());

        //current video mode
        v8::Local<v8::Object> modeObj = Nan::New<v8::Object>();

        Nan::Set(modeObj, Nan::New("width").ToLocalChecked(), Nan::New<v8::Integer>(mode.width));
        Nan::Set(modeObj, Nan::New("height").ToLocalChecked(), Nan::New<v8::Integer>(mode.height));
        Nan::Set(modeObj, Nan::New("redBits").ToLocalChecked(), Nan::New<v8::Integer>(mode.redBits));
        Nan::Set(modeObj, Nan::New("blueBits").ToLocalChecked(), Nan::New<v8::Integer>(mode.blueBits));
        Nan::Set(modeObj, Nan::New("greenBits").ToLocalChecked(), Nan::New<v8::Integer>(mode.greenBits));
        Nan::Set(modeObj, Nan::New("refreshRate").ToLocalChecked(), Nan::New<v8::Integer>(mode.refreshRate));

        Nan::Set(obj, Nan::New("mode").ToLocalChecked(), modeObj);

        //mmWidth, mmHeight
        int mmWidth, mmHeight;

        glfwGetMonitorPhysicalSize(monitor, &mmWidth, &mmHeight);

        Nan::Set(obj, Nan::New("mmWidth").ToLocalChecked(), Nan::New<v8::Integer>(mmWidth));
        Nan::Set(obj, Nan::New("mmHeight").ToLocalChecked(), Nan::New<v8::Integer>(mmHeight));

        //xScale, yScale
        float xScale, yScale;

        glfwGetMonitorContentScale(monitor, &xScale, &yScale);

        Nan::Set(obj, Nan::New("xScale").ToLocalChecked(), Nan::New<v8::Number>(xScale));
        Nan::Set(obj, Nan::New("yScale").ToLocalChecked(), Nan::New<v8::Number>(yScale));

        //workArea
        v8::Local<v8::Object> workAreaObj = Nan::New<v8::Object>();
        int x, y, w, h;

        glfwGetMonitorWorkarea(monitor, &x, &y, &w, &h);

        Nan::Set(workAreaObj, Nan::New("xPos").ToLocalChecked(), Nan::New<v8::Integer>(x));
        Nan::Set(workAreaObj, Nan::New("yPos").ToLocalChecked(), Nan::New<v8::Integer>(y));
        Nan::Set(workAreaObj, Nan::New("width").ToLocalChecked(), Nan::New<v8::Integer>(w));
        Nan::Set(workAreaObj, Nan::New("height").ToLocalChecked(), Nan::New<v8::Integer>(h));

        Nan::Set(obj, Nan::New("workArea").ToLocalChecked(), workAreaObj);

        //videos modes
        int count = 0;
        const GLFWvidmode *modes = glfwGetVideoModes(monitor, &count);
        v8::Local<v8::Array> modesArr = Nan::New<v8::Array>();

        assert(modes);

        for (int i = 0; i < count; i++) {
            const GLFWvidmode *mode = &modes[i];
            v8::Local<v8::Object> modeObj = Nan::New<v8::Object>();

            Nan::Set(modeObj, Nan::New("width").ToLocalChecked(), Nan::New<v8::Integer>(mode->width));
            Nan::Set(modeObj, Nan::New("height").ToLocalChecked(), Nan::New<v8::Integer>(mode->height));
            Nan::Set(modeObj, Nan::New("redBits").ToLocalChecked(), Nan::New<v8::Integer>(mode->redBits));
            Nan::Set(modeObj, Nan::New("blueBits").ToLocalChecked(), Nan::New<v8::Integer>(mode->blueBits));
            Nan::Set(modeObj, Nan::New("greenBits").ToLocalChecked(), Nan::New<v8::Integer>(mode->greenBits));
            Nan::Set(modeObj, Nan::New("refreshRate").ToLocalChecked(), Nan::New<v8::Integer>(mode->refreshRate));

            Nan::Set(modesArr, Nan::New<v8::Uint32>(i), modeObj);
        }

        Nan::Set(obj, Nan::New("modes").ToLocalChecked(), modesArr);
    }

    /**
     * @brief Get the current video mode of a monitor.
     *
     * @param monitor
     * @return const GLFWvidmode*
     */
    const GLFWvidmode* getVideoMode(GLFWmonitor *monitor) {
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);

        assert(mode);

#ifdef MAC
        //GLFW macOS bug: returns invalid screen size (see https://github.com/glfw/glfw/issues/2028)
        //check size exists
        bool found = false;
        int count = 0;
        const GLFWvidmode *modes = glfwGetVideoModes(monitor, &count);

        for (int i = 0; i < count; i++) {
            const GLFWvidmode *mode2 = &modes[i];

            if (mode2->width == mode->width &&
                mode2->height == mode->height &&
                mode2->refreshRate == mode->refreshRate &&
                mode2->redBits == mode->redBits &&
                mode2->greenBits == mode->greenBits &&
                mode2->blueBits == mode->blueBits) {
                    found = true;
                    break;
                }
        }

        if (!found) {
            //use highest resolution
            mode = &modes[count - 1];
        }
#endif

        return mode;
    }

    /**
     * @brief Get the all available monitors.
     *
     * @param array
     */
    void getAllMonitors(v8::Local<v8::Array> &array) override {
        int count = 0;
        GLFWmonitor **monitors = glfwGetMonitors(&count);

        for (int i = 0; i < count; i++) {
            //see https://www.glfw.org/docs/3.3/group__monitor.html
            GLFWmonitor *monitor = monitors[i];
            v8::Local<v8::Value> obj = Nan::Null();

            getMonitorInfo(monitor, obj);

            Nan::Set(array, Nan::New<v8::Uint32>(i), obj);
        }
    }

    /**
     * @brief Use a monitor and display mode.
     *
     * @param obj
     */
    std::string setMonitor(v8::Local<v8::Object> &obj) override {
        //check fullscreen
        if (!this->monitor) {
            return "not in fullscreen mode";
        }

        //monitor
        Nan::MaybeLocal<v8::Value> monitorMaybe = Nan::Get(obj, Nan::New<v8::String>("monitor").ToLocalChecked());
        GLFWmonitor *monitor = NULL;

        if (!monitorMaybe.IsEmpty()) {
            v8::Local<v8::Value> monitorValue = monitorMaybe.ToLocalChecked();

            if (monitorValue->IsObject()) {
                v8::Local<v8::Object> monitorObj = Nan::To<v8::Object>(monitorValue).ToLocalChecked();

                //get monitor name
                Nan::MaybeLocal<v8::Value> nameMaybe = Nan::Get(monitorObj, Nan::New<v8::String>("name").ToLocalChecked());

                if (!nameMaybe.IsEmpty()) {
                    v8::Local<v8::Value> nameValue = nameMaybe.ToLocalChecked();

                    if (nameValue->IsString()) {
                        std::string name = AminoJSObject::toString(nameValue);

                        monitor = getMonitorByName(name);

                        if (!monitor) {
                            return "monitor not found";
                        }
                    } else {
                        return "name not string";
                    }
                } else {
                    return "name missing";
                }
            } else if (!monitorValue->IsUndefined()) {
                return "monitor is not an object";
            }
        }

        if (!monitor) {
            //use current
            monitor = this->monitor;
        }

        //mode
        Nan::MaybeLocal<v8::Value> modeMaybe = Nan::Get(obj, Nan::New<v8::String>("mode").ToLocalChecked());
        const GLFWvidmode *mode = NULL;

        if (!modeMaybe.IsEmpty()) {
            v8::Local<v8::Value> modeValue = modeMaybe.ToLocalChecked();

            if (modeValue->IsObject()) {
                v8::Local<v8::Object> modeObj = Nan::To<v8::Object>(modeValue).ToLocalChecked();

                //get integer values
                int width       = Nan::To<v8::Int32>(Nan::Get(modeObj, Nan::New<v8::String>("width").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
                int height      = Nan::To<v8::Int32>(Nan::Get(modeObj, Nan::New<v8::String>("height").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
                int redBits     = Nan::To<v8::Int32>(Nan::Get(modeObj, Nan::New<v8::String>("redBits").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
                int blueBits    = Nan::To<v8::Int32>(Nan::Get(modeObj, Nan::New<v8::String>("blueBits").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
                int greenBits   = Nan::To<v8::Int32>(Nan::Get(modeObj, Nan::New<v8::String>("greenBits").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();
                int refreshRate = Nan::To<v8::Int32>(Nan::Get(modeObj, Nan::New<v8::String>("refreshRate").ToLocalChecked()).ToLocalChecked()).ToLocalChecked()->Value();

                //find mode
                int count = 0;
                const GLFWvidmode *modes = glfwGetVideoModes(monitor, &count);

                assert(modes);
                assert(count);

                for (int i = 0; i < count; i++) {
                    const GLFWvidmode *mode2 = &modes[i];

                    if (mode2->width == width &&
                        mode2->height == height &&
                        mode2->redBits == redBits &&
                        mode2->blueBits == blueBits &&
                        mode2->greenBits == greenBits &&
                        mode2->refreshRate == refreshRate) {
                            mode = mode2;
                            break;
                        }
                }
            } else if (!modeValue->IsUndefined()) {
                return "mode is not an object";
            }
        }

        //apply
        if (!mode) {
            //use current mode
            mode = getVideoMode(monitor);
        }

        this->monitor = monitor;
        this->mode = *mode;

        return "";
    }

    /**
     * @brief Find a monitor by name.
     *
     * @param name
     * @return GLFWmonitor*
     */
    GLFWmonitor* getMonitorByName(std::string name) {
        int count = 0;
        GLFWmonitor **monitors = glfwGetMonitors(&count);

        for (int i = 0; i < count; i++) {
            //see https://www.glfw.org/docs/3.3/group__monitor.html
            GLFWmonitor *monitor = monitors[i];
            std::string name2 = std::string(glfwGetMonitorName(monitor));

            if (name == name2) {
                return monitor;
            }
        }

        return NULL;
    }

    /**
     * Get default monitor resolution.
     */
    bool getScreenInfo(int &w, int &h, int &refreshRate, bool &fullscreen) override {
        //debug
        //printf("getScreenInfo\n");

        //get monitor properties
        GLFWmonitor *usedMonitor = monitor ? monitor:glfwGetPrimaryMonitor();

        assert(usedMonitor);

        //get video mode
        const GLFWvidmode *vidmode = getVideoMode(usedMonitor);

        assert(vidmode);

        w = vidmode->width;
        h = vidmode->height;
        refreshRate = vidmode->refreshRate;
        fullscreen = monitor != NULL;

        return true;
    }

    /**
     * Add GLFW properties.
     */
    void populateRuntimeProperties(v8::Local<v8::Object> &obj) override {
        //debug
        //printf("populateRuntimeProperties\n");

        AminoGfx::populateRuntimeProperties(obj);

        //GLFW
        Nan::Set(obj, Nan::New("glfwVersion").ToLocalChecked(), Nan::New(std::string(glfwGetVersionString())).ToLocalChecked());

        //retina scale factor
        Nan::Set(obj, Nan::New("scale").ToLocalChecked(), Nan::New<v8::Integer>(viewportW / (int)propW->value));
    }

    /**
     * Initialize GLFW window.
     */
    void initRenderer() override {
        //base
        AminoGfx::initRenderer();

        /*
         * use OpenGL 2.x
         *
         * OpenGL 3.2 changes:
         *
         *   - https://developer.apple.com/library/mac/documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/UpdatinganApplicationtoSupportOpenGL3/UpdatinganApplicationtoSupportOpenGL3.html#//apple_ref/doc/uid/TP40001987-CH3-SW1
         *
         * Thread safety: http://www.glfw.org/docs/latest/intro.html#thread_safety
         */

        //fullscreen support
        int windowW = propW->value;
        int windowH = propH->value;

        if (monitor && mode.width && mode.height && mode.refreshRate) {
            //use specific mode (otherwise the width/height are used to get closest mode)
            glfwWindowHint(GLFW_RED_BITS, mode.redBits);
            glfwWindowHint(GLFW_GREEN_BITS, mode.greenBits);
            glfwWindowHint(GLFW_BLUE_BITS, mode.blueBits);
            glfwWindowHint(GLFW_REFRESH_RATE, mode.refreshRate);

            windowW = mode.width;
            windowH = mode.height;
        }

        //create window
        glfwWindowHint(GLFW_DEPTH_BITS, 32); //default
        glfwWindowHint(GLFW_SAMPLES, 4); //increase quality

        window = glfwCreateWindow(windowW, windowH, propTitle->value.c_str(), monitor, NULL);

        if (!window) {
            //exit on error
            printf("couldn't open a window. quitting\n");

            glfwTerminate();
            exit(EXIT_FAILURE);
        }

        //store
        windowMap->insert(std::pair<GLFWwindow *, AminoGfxGlfw *>(window, this));

        //check window size
        glfwGetWindowSize(window, &windowW, &windowH);

        if (DEBUG_GLFW) {
            printf("window size: requested=%ix%i, got=%ix%i\n", (int)propW->value, (int)propH->value, windowW, windowH);
        }

        if (monitor) {
            //set to new monitor value (Note: calls updateSize() internally)
            updateScreenProperty();
        } else {
            updateSize(windowW, windowH);
        }

        //check window pos
        if (propX->value != -1 && propY->value != -1) {
            //use initial position
            glfwSetWindowPos(window, propX->value, propY->value);
        }

        int windowX;
        int windowY;

        glfwGetWindowPos(window, &windowX, &windowY);

        updatePosition(windowX, windowY);

        //get framebuffer size
        glfwGetFramebufferSize(window, &viewportW, &viewportH);
        viewportChanged = true;

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }

        //activate context (needed by JS code to create shaders)
        glfwMakeContextCurrent(window);

        //swap interval
        if (swapInterval == 0) {
            //limit to screen (others as fast as possible)
            swapInterval = 1;
        }

        //debug
        //printf("swap interval: %i\n", (int)swapInterval);

        glfwSwapInterval(swapInterval);

        //set bindings
        //Note: no touch support (see https://github.com/glfw/glfw/issues/42)
        glfwSetKeyCallback(window, handleKeyEvents);
        glfwSetCursorPosCallback(window, handleMouseMoveEvents);
        glfwSetMouseButtonCallback(window, handleMouseClickEvents);
        glfwSetScrollCallback(window, handleMouseWheelEvents);
        glfwSetWindowSizeCallback(window, handleWindowSizeChanged);
        glfwSetWindowPosCallback(window, handleWindowPosChanged);
        glfwSetWindowCloseCallback(window, handleWindowCloseEvent);

        //hints
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
    }

    /**
     * Error handler.
     */
    static void handleErrors(int error, const char *description) {
        printf("GLFW error: %i %s\n", error, description);
    }

    /**
     * @brief Handle monitor events.
     *
     * @param monitor
     * @param event
     */
    static void handleMonitorEvents(GLFWmonitor* monitor, int event) {
        //handle disconnected
        if (event == GLFW_DISCONNECTED) {
            //running now in windowed mode
            //monitor and mode pointers get invalid after this call returns

            if (DEBUG_GLFW) {
                printf("-> monitor disconnected: %s\n", glfwGetMonitorName(monitor));
            }

            //find affected renderers
            for (auto const &item : *windowMap) {
                if (monitor == item.second->monitor) {
                    //reset (Note: keeping mode)
                    item.second->monitor = NULL;
                    item.second->lastMonitor = glfwGetMonitorName(monitor);

                    if (DEBUG_GLFW) {
                        printf("-> removed monitor from renderer\n");
                    }
                }
            }

            return;
        }

        //handle connected
        if (event == GLFW_CONNECTED) {
            if (DEBUG_GLFW) {
                printf("-> monitor connected: %s\n", glfwGetMonitorName(monitor));
            }

            //find renderer
            std::string name = glfwGetMonitorName(monitor);

            for (auto const &item : *windowMap) {
                AminoGfxGlfw *gfx = item.second;

                if (name == gfx->lastMonitor) {
                    //re-use last monitor
                    gfx->monitor = monitor;
                    glfwSetWindowMonitor(gfx->window, gfx->monitor, 0, 0, gfx->mode.width, gfx->mode.height, gfx->mode.refreshRate);

                    if (DEBUG_GLFW) {
                        printf("-> using monitor again\n");
                    }
                }
            }

            return;
        }
    }

    /**
     * Key event.
     *
     * Note: called on main thread.
     */
    static void handleKeyEvents(GLFWwindow *window, int key, int scancode, int action, int mods) {
        //TODO Unicode character support: http://www.glfw.org/docs/latest/group__input.html#gabf24451c7ceb1952bc02b17a0d5c3e5f

        AminoGfxGlfw *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //debug
        //printf("key event: key=%i scancode=%i\n", key, scancode);

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        if (action == GLFW_RELEASE) {
            //release
            Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("key.release").ToLocalChecked());
        } else if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            //press or repeat
            Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("key.press").ToLocalChecked());
        }

        //key codes
        Nan::Set(event_obj, Nan::New("keycode").ToLocalChecked(), Nan::New(key));
        Nan::Set(event_obj, Nan::New("scancode").ToLocalChecked(), Nan::New(scancode));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse moved.
     *
     * Note: called on main thread.
     */
    static void handleMouseMoveEvents(GLFWwindow *window, double x, double y) {
        AminoGfxGlfw *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //debug
        //printf("mouse moved event: %f %f\n", x, y);

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),  Nan::New("mouse.position").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("x").ToLocalChecked(),     Nan::New(x));
        Nan::Set(event_obj, Nan::New("y").ToLocalChecked(),     Nan::New(y));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse click event.
     *
     * Note: called on main thread.
     */
    static void handleMouseClickEvents(GLFWwindow *window, int button, int action, int mods) {
        AminoGfxGlfw *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //debug
        if (DEBUG_GLFW) {
            printf("mouse clicked event: %i %i\n", button, action);
        }

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("mouse.button").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("button").ToLocalChecked(), Nan::New(button));
        Nan::Set(event_obj, Nan::New("state").ToLocalChecked(),  Nan::New(action));

        obj->fireEvent(event_obj);
    }

    /**
     * Mouse wheel event.
     *
     * Note: called on main thread.
     */
    static void handleMouseWheelEvents(GLFWwindow *window, double xoff, double yoff) {
        AminoGfxGlfw *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("mousewheel.v").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("xoff").ToLocalChecked(),   Nan::New(xoff));
        Nan::Set(event_obj, Nan::New("yoff").ToLocalChecked(),   Nan::New(yoff));

        obj->fireEvent(event_obj);
    }

    /**
     * Window size has changed.
     */
    static void handleWindowSizeChanged(GLFWwindow *window, int newWidth, int newHeight) {
        if (DEBUG_GLFW) {
            printf("handleWindowSizeChanged() %ix%i\n", newWidth, newHeight);
        }

        AminoGfxGlfw *obj = windowToInstance(window);

        assert(obj);

        obj->handleWindowSizeChanged(newWidth, newHeight);
    }

    /**
     * Internal event handler.
     *
     * Note: called on main thread.
     */
    void handleWindowSizeChanged(int newWidth, int newHeight) {
        //check size
        if (propW->value == newWidth && propH->value == newHeight) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //update
        updateSize(newWidth, newHeight);

        //debug
        if (DEBUG_GLFW) {
            printf("window size: %ix%i\n", newWidth, newHeight);
        }

        //get framebuffer size
        assert(window);

        glfwGetFramebufferSize(window, &viewportW, &viewportH);
        viewportChanged = true;

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(),   Nan::New("window.size").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("width").ToLocalChecked(),  Nan::New(propW->value));
        Nan::Set(event_obj, Nan::New("height").ToLocalChecked(), Nan::New(propH->value));

        //fire
        fireEvent(event_obj);
    }

    /**
     * Window position has changed.
     */
    static void handleWindowPosChanged(GLFWwindow *window, int newX, int newY) {
        if (DEBUG_GLFW) {
            printf("handleWindowPosChanged() %ix%i\n", newX, newY);
        }

        AminoGfxGlfw *obj = windowToInstance(window);

        assert(obj);

        obj->handleWindowPosChanged(newX, newY);
    }

    /**
     * Internal event handler.
     *
     * Note: called on main thread.
     */
    void handleWindowPosChanged(int newX, int newY) {
        //check size
        if (propX->value == newX && propY->value == newY) {
            return;
        }

        //create scope
        Nan::HandleScope scope;

        //update
        updatePosition(newX, newY);

        //debug
        if (DEBUG_GLFW) {
            printf("window position: %ix%i\n", newX, newY);
        }

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("window.pos").ToLocalChecked());
        Nan::Set(event_obj, Nan::New("x").ToLocalChecked(),    Nan::New(propX->value));
        Nan::Set(event_obj, Nan::New("y").ToLocalChecked(),    Nan::New(propY->value));

        //fire
        fireEvent(event_obj);
    }

    /**
     * Window close event.
     *
     * Note: called on main thread.
     * Note: window stays open.
     */
    static void handleWindowCloseEvent(GLFWwindow *window) {
        AminoGfxGlfw *obj = windowToInstance(window);

        assert(obj);

        //create scope
        Nan::HandleScope scope;

        //create object
        v8::Local<v8::Object> event_obj = Nan::New<v8::Object>();

        Nan::Set(event_obj, Nan::New("type").ToLocalChecked(), Nan::New("window.close").ToLocalChecked());

        //fire
        obj->fireEvent(event_obj);
    }

    void start() override {
        //ready to get control back to JS code
        ready();

        //detach context from main thread
        glfwMakeContextCurrent(NULL);
    }

    bool bindContext() override {
        //bind OpenGL context
        if (!window) {
            return false;
        }

        glfwMakeContextCurrent(window);

        return true;
    }

    void renderingDone() override {
        //debug
        //printf("renderingDone()\n");

        //swap buffer
        assert(window);

        glfwSwapBuffers(window);
    }

    void handleSystemEvents() override {
        //handle events
        glfwPollEvents();
    }

    /**
     * Update the window size.
     *
     * Note: has to be called on main thread
     */
    void updateWindowSize() override {
        //debug
        //printf("updateWindowSize()\n");

        //ignore size changes before window is created
        if (!window) {
            return;
        }

        //Note: getting size changed event
        glfwSetWindowSize(window, propW->value, propH->value);

        //get framebuffer size
        glfwGetFramebufferSize(window, &viewportW, &viewportH);
        viewportChanged = true;

        //check framebuffer size
        if (DEBUG_GLFW) {
            printf("framebuffer size: %ix%i\n", viewportW, viewportH);
        }
    }

    /**
     * Update the window position.
     *
     * Note: has to be called on main thread
     */
    void updateWindowPosition() override {
        //ignore position changes before window is created
        if (!window) {
            return;
        }

        if (DEBUG_GLFW) {
            printf("updateWindowPosition() %i %i\n", (int)propX->value, (int)propY->value);
        }

        glfwSetWindowPos(window, propX->value, propY->value);
    }

    /**
     * Update the title.
     *
     * Note: has to be called on main thread
     */
    void updateWindowTitle() override {
        //ignore title changes before window is created
        if (!window) {
            return;
        }

        glfwSetWindowTitle(window, propTitle->value.c_str());
    }

    /**
     * Shared atlas texture has changed.
     */
    void atlasTextureHasChanged(texture_atlas_t *atlas) override {
        //check single instance case
        if (instanceCount == 1) {
            return;
        }

        //run on main thread
        enqueueJSCallbackUpdate(static_cast<jsUpdateCallback>(&AminoGfxGlfw::atlasTextureHasChangedHandler), NULL, atlas);
    }

    /**
     * Handle on main thread.
     */
    void atlasTextureHasChangedHandler(JSCallbackUpdate *update) {
        AminoGfx *gfx = static_cast<AminoGfx *>(update->obj);
        texture_atlas_t *atlas = (texture_atlas_t *)update->data;

        for (auto const &item : *windowMap) {
            if (gfx == item.second) {
                continue;
            }

            item.second->updateAtlasTexture(atlas);
        }
    }

    /**
     * Create video player.
     */
    AminoVideoPlayer *createVideoPlayer(AminoTexture *texture, AminoVideo *video) override {
        return new AminoGlfwVideoPlayer(texture, video);
    }
};

//static initializers
bool AminoGfxGlfw::glfwInitialized = false;
std::map<GLFWwindow *, AminoGfxGlfw *> *AminoGfxGlfw::windowMap = new std::map<GLFWwindow *, AminoGfxGlfw *>();

//
// AminoGfxGlfwFactory
//

/**
 * Create AminoGfx factory.
 */
AminoGfxGlfwFactory::AminoGfxGlfwFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoGfx", callback) {
    //empty
}

/**
 * Create AminoGfx instance.
 */
AminoJSObject* AminoGfxGlfwFactory::create() {
    return new AminoGfxGlfw();
}

//
// AminoGlfwVideoPlayer
//

AminoGlfwVideoPlayer::AminoGlfwVideoPlayer(AminoTexture *texture, AminoVideo *video): AminoVideoPlayer(texture, video) {
    //semaphore
    int res = uv_sem_init(&pauseSem, 0);

    assert(res == 0);

    //lock
    uv_mutex_init(&frameLock);
}

AminoGlfwVideoPlayer::~AminoGlfwVideoPlayer() {
    closeDemuxer();

    //semaphore
    uv_sem_destroy(&pauseSem);

    //lock
    uv_mutex_destroy(&frameLock);
}

/**
 * Initialize the stream (on main thread).
 */
bool AminoGlfwVideoPlayer::initStream() {
    //get file name
    filename = video->getPlaybackSource();
    options = video->getPlaybackOptions();

    return true;
}

/**
 * Initialize the video player (on the rendering thread).
 */
void AminoGlfwVideoPlayer::init() {
    if (DEBUG_VIDEOS) {
        printf("GLFW Video Player: init()\n");
    }

    //initialize demuxer
    assert(filename.length());

    demuxer = new VideoDemuxer();

    if (!demuxer->init()) {
        lastError = demuxer->getLastError();
        delete demuxer;
        demuxer = NULL;

        handleInitDone(false);

        return;
    }

    //create demuxer thread
    int res = uv_thread_create(&thread, demuxerThread, this);

    assert(res == 0);

    threadRunning = true;
}

/**
 * Demuxer thread.
 */
void AminoGlfwVideoPlayer::demuxerThread(void *arg) {
    AminoGlfwVideoPlayer *player = static_cast<AminoGlfwVideoPlayer *>(arg);

    assert(player);

    //init demuxer
    player->initDemuxer();

    //Note: demuxer not closed

    //done
    player->threadRunning = false;
}

/**
 * Init demuxer.
 */
void AminoGlfwVideoPlayer::initDemuxer() {
    if (DEBUG_VIDEOS) {
        printf("GLFW Video Player: initDemuxer()\n");
    }

    assert(demuxer);

    //load file
    if (!demuxer->loadFile(filename, options)) {
        lastError = demuxer->getLastError();
        handleInitDone(false);
        return;
    }

    //set video size
    videoW = demuxer->width;
    videoH = demuxer->height;

    assert(videoW > 0);
    assert(videoH > 0);

    if (DEBUG_VIDEOS) {
        printf("-> initializing video stream\n");
    }

    //initialize stream
    if (!demuxer->initStream()) {
        lastError = demuxer->getLastError();
        handleInitDone(false);
        return;
    }

    if (DEBUG_VIDEOS) {
        printf("-> read first frame of video\n");
    }

//cbxx last cool pi 4 output
//cbxx FIXME swscaler: no accelerated colorspace conversion found from yuv420 to rgb24
//cbxx FIXME assertion !persistent().IsEmpty() failed -> nan_object_wrap.h #93 => AminoTexture

    //read first frame
    double timeStart;
    READ_FRAME_RESULT res = demuxer->readDecodedFrame(timeStart);
    double timeStartSys = getTime() / 1000;

    if (res == READ_END_OF_VIDEO) {
        lastError = "empty video";
        handleInitDone(false);
        return;
    }

    if (res == READ_ERROR) {
        lastError = "could not load video stream";
        handleInitDone(false);
        return;
    }

    if (DEBUG_VIDEOS) {
        printf("-> initializing video texture\n");
    }

    //switch to renderer thread
    texture->initVideoTexture();

    if (DEBUG_VIDEOS) {
        printf("-> starting video playback loop\n");
    }

    //playback loop
    while (true) {
        //check stop
        if (doStop) {
            //end playback
            handlePlaybackStopped();
            return;
        }

        //check pause
        if (doPause) {
            double pauseTime = getTime() / 1000;

            demuxer->pause();
            handlePlaybackPaused();

            //wait
            uv_sem_wait(&pauseSem);
            doPause = false;

            if (!doStop) {
                //change state
                demuxer->resume();
                handlePlaybackResumed();

                //change time
                double resumeTime = getTime() / 1000;

                timeStartSys += resumeTime - pauseTime;
            }

            //next
            continue;
        }

        //next frame
        double time;
        int res = demuxer->readDecodedFrame(time);
        double timeSys = getTime() / 1000;

        if (res == READ_ERROR) {
            if (DEBUG_VIDEOS) {
                printf("-> read error\n");
            }

            handlePlaybackError();
            return;
        }

        if (res == READ_END_OF_VIDEO) {
            if (DEBUG_VIDEOS) {
                printf("-> end of video\n");
            }

            if (loop > 0) {
                loop--;
            }

            if (loop == 0) {
                //end playback
                handlePlaybackDone();
                return;
            }

            //rewind
            if (!demuxer->rewindDecoder(timeStart)) {
                handlePlaybackError();
                return;
            }

            timeStartSys = getTime() / 1000;
            timeSys = timeStartSys;

            time = timeStart;

            handleRewind();

            if (DEBUG_VIDEOS) {
                printf("-> rewind\n");
            }
        }

        //correct timing
        if (!demuxer->realtime) {
            double timeSleep = (time - timeStart) - (timeSys - timeStartSys);

            if (timeSleep > 0) {
                usleep(timeSleep * 1000000);

                if (DEBUG_VIDEO_TIMING) {
                    printf("sleep: %f ms\n", timeSleep * 1000);
                }
            }
        }

        //show
        demuxer->switchDecodedFrame();

        //update media time
        mediaTime = getTime() / 1000 - timeStartSys;
    }
}

/**
 * Free the demuxer instance (on main thread).
 */
void AminoGlfwVideoPlayer::closeDemuxer() {
    //stop playback
    stopPlayback();

    //wait for thread
    if (threadRunning) {
        int res = uv_thread_join(&thread);

        assert(res == 0);
    }

    //free demuxer
    if (demuxer) {
        uv_mutex_lock(&frameLock);

        delete demuxer;
        demuxer = NULL;

        uv_mutex_unlock(&frameLock);
    }
}

/**
 * Init video texture on OpenGL thread.
 */
void AminoGlfwVideoPlayer::initVideoTexture() {
    if (DEBUG_VIDEOS) {
        printf("video: init video texture\n");
    }

    if (!initTexture()) {
        handleInitDone(false);
        return;
    }

    //done
    handleInitDone(true);
}

/**
 * Init texture.
 */
bool AminoGlfwVideoPlayer::initTexture() {
    glBindTexture(GL_TEXTURE_2D, texture->getTexture());

    //size (has to be equal to video dimension!)
    GLsizei textureW = videoW;
    GLsizei textureH = videoH;

    assert(demuxer);

    GLvoid *data = demuxer->getFrameRGBData(frameId);

    assert(data);
    assert(textureW > 0);
    assert(textureH > 0);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureW, textureH, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return true;
}

/**
 * Update the texture (on rendering thread).
 */
void AminoGlfwVideoPlayer::updateVideoTexture(GLContext *ctx) {
    uv_mutex_lock(&frameLock);

    if (!demuxer) {
        uv_mutex_unlock(&frameLock);
        return;
    }

    //FIXME crashes observed somewhere here

    //get current frame
    int id;
    GLvoid *data = demuxer->getFrameRGBData(id);

    if (!data) {
        uv_mutex_unlock(&frameLock);
        return;
    }

    if (id == frameId) {
        //debug
        //printf("skipping frame\n");

        uv_mutex_unlock(&frameLock);
        return;
    }

    frameId = id;

    glBindTexture(GL_TEXTURE_2D, texture->getTexture());

    GLsizei textureW = videoW;
    GLsizei textureH = videoH;

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureW, textureH, GL_RGB, GL_UNSIGNED_BYTE, data);

    uv_mutex_unlock(&frameLock);
}

/**
 * Get current media time.
 */
double AminoGlfwVideoPlayer::getMediaTime() {
    if (playing || paused) {
        return mediaTime;
    }

    return -1;
}

/**
 * Get video duration (-1 if unknown).
 */
double AminoGlfwVideoPlayer::getDuration() {
    if (demuxer) {
        return demuxer->durationSecs;
    }

    return -1;
}

/**
 * Get the framerate (0 if unknown).
 */
double AminoGlfwVideoPlayer::getFramerate() {
    if (demuxer) {
        return demuxer->fps;
    }

    return 0;
}

/**
 * Stop playback.
 */
void AminoGlfwVideoPlayer::stopPlayback() {
    if (!playing && !paused) {
        return;
    }

    //stop
    doStop = true;

    if (paused) {
        //resume thread
        uv_sem_post(&pauseSem);
    }
}

/**
 * Pause playback.
 */
bool AminoGlfwVideoPlayer::pausePlayback() {
    if (!playing) {
        return true;
    }

    //pause
    doPause = true;

    return true;
}

/**
 * Resume (stopped) playback.
 */
bool AminoGlfwVideoPlayer::resumePlayback() {
    if (!paused) {
        return true;
    }

    //resume thread
    uv_sem_post(&pauseSem);

    return true;
}

//
// Exit handler
//

void exitHandler(void *arg) {
    //Note: not called on Ctrl-C (use process.on('SIGINT', ...))
    if (DEBUG_BASE) {
        printf("app exiting\n");
    }
}

void cleanupHook(void*) {
    if (DEBUG_BASE) {
        printf("cleanup hook called\n");
    }
}

/**
 * Show crash details.
 */
void crashHandler(int sig) {
    void *array[10];
    size_t size;

    //process & thread
    pid_t pid = getpid();
#ifdef LINUX
    pid_t tid = gettid();
#endif
#ifdef MAC
    uint64_t tid;
    bool mainThread = pthread_main_np() != 0;

    pthread_threadid_np(NULL, &tid);
#endif
    uv_thread_t threadId = uv_thread_self();

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
#ifdef MAC
    fprintf(stderr, "Error: signal %d (process=%d, thread=%i, uvThread=%lu, main=%s):\n", sig, pid, (int)tid, (unsigned long)threadId, mainThread ? "yes":"no");
#endif
#ifdef LINUX
    fprintf(stderr, "Error: signal %d (process=%d, thread=%d, uvThread=%lu):\n", sig, pid, tid, (unsigned long)threadId);
#endif
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(1);
}

// ========== Event Callbacks ===========

NAN_MODULE_INIT(InitAll) {
    //crash handler (comment to get macOS crash dialog)
    signal(SIGSEGV, crashHandler);

    //main class
    AminoGfxGlfw::Init(target);

    //amino classes
    AminoGfx::InitClasses(target);

    //exit handler

    //FIXME cannot get node::Environment* instance
    //node::AtExit(exitHandler); //deprecated
    //node::AtExit(info.Env(), exitHandler, nullptr);

    v8::Isolate *isolate = v8::Isolate::GetCurrent();

    node::AddEnvironmentCleanupHook(isolate, cleanupHook, nullptr);
}

//entry point
NODE_MODULE(aminonative, InitAll)
