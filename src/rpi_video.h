#ifndef _AMINO_RPI_VIDEO_H
#define _AMINO_RPI_VIDEO_H

#include <queue>

#include "base.h"

extern "C" {
    #include "ilclient/ilclient.h"
    #include "EGL/eglext.h"
}

#ifdef EGL_DISPMANX
#define USE_OMX_VCOS_THREAD
#endif

/**
 * OMX Video Player.
 */
class AminoOmxVideoPlayer : public AminoVideoPlayer {
public:
    AminoOmxVideoPlayer(AminoTexture *texture, AminoVideo *video);
    ~AminoOmxVideoPlayer();

    void init() override;
    int getNeededTextures() override;
    bool initTexture();

    void destroy() override;
    void destroyAminoOmxVideoPlayer();

    //stream
    bool initStream() override;
    void closeStream();

    //OMX
#ifdef USE_OMX_VCOS_THREAD
    static void* omxThread(void *arg);
    static void* decoderThread(void *arg);
#else
    static void omxThread(void *arg);
    static void decoderThread(void *arg);
#endif
    static void handleFillBufferDone(void *data, COMPONENT_T *comp);

    bool initOmx();
    int playOmx();
    void stopOmx();
    void destroyOmx();

    void initVideoTexture() override;
    void updateVideoTexture(GLContext *ctx) override;
    bool setupOmxTexture();
    bool omxFillNextEglBuffer();

    //metadata
    double getMediaTime() override;
    double getDuration() override;
    double getFramerate() override;
    void stopPlayback() override;
    bool pausePlayback() override;
    bool resumePlayback() override;

    OMX_U32 getOmxFramerate();
    bool setOmxSpeed(OMX_S32 speed);

    //software decoder
    void initDemuxer();

private:
    EGLImageKHR *eglImages = NULL;
    bool eglImagesReady = false;
    ILCLIENT_T *client = NULL;
    AnyVideoStream *stream = NULL;

#ifdef USE_OMX_VCOS_THREAD
    VCOS_THREAD_T thread;
#else
    uv_thread_t thread;
#endif
    bool threadRunning = false;
    bool softwareDecoding = false;

    TUNNEL_T tunnel[4];
    COMPONENT_T *list[5];
    bool omxDestroyed = false;
    bool doStop = false;
    bool doPause = false;
    double pauseTime;
    uv_sem_t pauseSem;
    uv_sem_t textureSem;

    int frameId = -1;

    static std::string getOmxError(OMX_S32 err);
    static void omxErrorHandler(void *userData, COMPONENT_T *comp, OMX_U32 data);

    bool showOmxBufferInfo(COMPONENT_T *comp, int port);
    bool setOmxBufferCount(COMPONENT_T *comp, int port, int count);

public:
    COMPONENT_T *egl_render = NULL;
    OMX_BUFFERHEADERTYPE **eglBuffers = NULL;
    uv_mutex_t bufferLock;
    uv_mutex_t destroyLock;

    int textureActive = 0;
    int textureFilling = 1;
    std::queue<int> textureReady;
    std::queue<int> textureNew;

    bool bufferFilled = false;
    bool bufferError = false;

    double mediaTime = -1;
    double timeStartSys = -1;
};

#endif