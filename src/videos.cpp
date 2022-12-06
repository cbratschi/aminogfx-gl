#include "videos.h"
#include "base.h"
#include "images.h"

#include <sstream>

#define DEBUG_VIDEO_FRAMES false
#define DEBUG_VIDEO_STREAM false

//
// AminoVideo
//

/**
 * Constructor.
 */
AminoVideo::AminoVideo(): AminoJSObject(getFactory()->name) {
    //empty
}

/**
 * Destructor.
 */
AminoVideo::~AminoVideo()  {
    //empty
}

/**
 * Get playback loop value.
 */
bool AminoVideo::getPlaybackLoop(int32_t &loop) {
    Nan::MaybeLocal<v8::Value> loopValue = Nan::Get(handle(), Nan::New<v8::String>("loop").ToLocalChecked());

    if (loopValue.IsEmpty()) {
        return false;
    }

    v8::Local<v8::Value> loopLocal = loopValue.ToLocalChecked();

    if (loopLocal->IsBoolean()) {
        if (Nan::To<v8::Boolean>(loopLocal).ToLocalChecked()->Value()) {
            loop = -1;
        } else {
            loop = 0;
        }

        return true;
    } else if (loopLocal->IsInt32()) {
        loop = Nan::To<v8::Int32>(loopLocal).ToLocalChecked()->Value();

        if (loop < 0) {
            loop = -1;
        }

        return true;
    }

    return false;
}

/**
 * Get local file name.
 *
 * Note: must be called on main thread!
 */
std::string AminoVideo::getPlaybackSource() {
    Nan::MaybeLocal<v8::Value> srcValue = Nan::Get(handle(), Nan::New<v8::String>("src").ToLocalChecked());

    if (srcValue.IsEmpty()) {
        return "";
    }

    v8::Local<v8::Value> srcLocal = srcValue.ToLocalChecked();

    if (srcLocal->IsString()) {
        //file or URL
        return AminoJSObject::toString(srcLocal);
    }

    return "";
}

/**
 * Get playback options (FFmpeg/libav).
 *
 * Note: must be called on main thread!
 */
std::string AminoVideo::getPlaybackOptions() {
    Nan::MaybeLocal<v8::Value> optsValue = Nan::Get(handle(), Nan::New<v8::String>("opts").ToLocalChecked());

    if (optsValue.IsEmpty()) {
        return "";
    }

    v8::Local<v8::Value> optsLocal = optsValue.ToLocalChecked();

    if (optsLocal->IsString()) {
        //playback options
        return AminoJSObject::toString(optsLocal);
    }

    return "";
}

/**
 * Get factory instance.
 */
AminoVideoFactory* AminoVideo::getFactory() {
    static AminoVideoFactory *aminoVideoFactory = NULL;

    if (!aminoVideoFactory) {
        aminoVideoFactory = new AminoVideoFactory(New);
    }

    return aminoVideoFactory;
}

/**
 * Add class template to module exports.
 */
NAN_MODULE_INIT(AminoVideo::Init) {
    if (DEBUG_VIDEOS) {
        printf("AminoVideo init\n");
    }

    AminoVideoFactory *factory = getFactory();
    v8::Local<v8::FunctionTemplate> tpl = AminoJSObject::createTemplate(factory);

    //prototype methods
    // none

    //global template instance
    Nan::Set(target, Nan::New(factory->name).ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

/**
 * JS object construction.
 */
NAN_METHOD(AminoVideo::New) {
    AminoJSObject::createInstance(info, getFactory());
}

//
//  AminoVideoFactory
//

/**
 * Create AminoVideo factory.
 */
AminoVideoFactory::AminoVideoFactory(Nan::FunctionCallback callback): AminoJSObjectFactory("AminoVideo", callback) {
    //empty
}

/**
 * Create AminoVideo instance.
 */
AminoJSObject* AminoVideoFactory::create() {
    return new AminoVideo();
}

//
//  AminoVideoPlayer
//

/**
 * Constructor.
 *
 * Note: has to be called on main thread!
 */
AminoVideoPlayer::AminoVideoPlayer(AminoTexture *texture, AminoVideo *video): texture(texture), video(video) {
    //playback settings
    video->getPlaybackLoop(loop);

    //keep instance
    video->retain();
}

/**
 * Destructor.
 */
AminoVideoPlayer::~AminoVideoPlayer() {
    //destroy player instance
    destroyAminoVideoPlayer();
}

/**
 * Get amount of textures needed by player.
 */
int AminoVideoPlayer::getNeededTextures() {
    return 1;
}

/**
 * Destroy the video player.
 */
void AminoVideoPlayer::destroy() {
    if (destroyed) {
        return;
    }

    destroyed = true;

    destroyAminoVideoPlayer();

    //overwrite

    //end of destroy chain
}

/**
 * Destroy the video player.
 */
void AminoVideoPlayer::destroyAminoVideoPlayer() {
    if (video) {
        video->release();
        video = NULL;
    }
}

/**
 * Check if player is ready.
 */
bool AminoVideoPlayer::isReady() {
    return ready;
}

/**
 * Check if playing.
 */
bool AminoVideoPlayer::isPlaying() {
    return playing;
}

/**
 * Check if paused.
 */
bool AminoVideoPlayer::isPaused() {
    return paused;
}

/**
 * Get last error.
 */
std::string AminoVideoPlayer::getLastError() {
    return lastError;
}

/**
 * Get video dimensions.
 */
void AminoVideoPlayer::getVideoDimension(int32_t &w, int32_t &h) {
    w = videoW;
    h = videoH;
}

/**
 * Get player state.
 */
std::string AminoVideoPlayer::getState() {
    if (!initDone) {
        return "loading";
    }

    if (failed) {
        return "error";
    } else if (playing) {
        return "playing";
    } else if (paused) {
        return "paused";
    }

    return "stopped";
}

/**
 * Playback ended.
 */
void AminoVideoPlayer::handlePlaybackDone() {
    if (destroyed) {
        return;
    }

    if (playing || paused) {
        playing = false;
        paused = false;

        if (DEBUG_VIDEOS) {
            printf("video: playback done\n");
        }

        if (!failed) {
            fireEvent("ended");
        }
    }
}

/**
 * Playback failed.
 */
void AminoVideoPlayer::handlePlaybackError() {
    if (destroyed) {
        return;
    }

    //set state
    if ((playing || paused) && !failed) {
        failed = true;

        if (DEBUG_VIDEOS) {
            printf("video: playback error\n");
        }

        fireEvent("error");
    }

    //done
    handlePlaybackDone();
}

/**
 * Playback was stopped.
 */
void AminoVideoPlayer::handlePlaybackStopped() {
    if (destroyed) {
        return;
    }

    //send event
    if (playing || paused) {
        if (DEBUG_VIDEOS) {
            printf("video: playback stopped\n");
        }

        //Note: not part of HTML5
        fireEvent("stop");
    }

    //done
    handlePlaybackDone();
}

/**
 * Playback was paused.
 */
void AminoVideoPlayer::handlePlaybackPaused() {
    if (destroyed) {
        return;
    }

    if (playing) {
        playing = false;
        paused = true;

        if (DEBUG_VIDEOS) {
            printf("video: playback paused\n");
        }

        fireEvent("pause");
    }
}

/**
 * Playback was resumed.
 */
void AminoVideoPlayer::handlePlaybackResumed() {
    if (destroyed) {
        return;
    }

    if (paused) {
        paused = false;
        playing = true;

        if (DEBUG_VIDEOS) {
            printf("video: playback resumed\n");
        }

        fireEvent("play");
    }
}

/**
 * Video is ready for playback.
 */
void AminoVideoPlayer::handleInitDone(bool ready) {
    if (destroyed || initDone) {
        return;
    }

    initDone = true;
    this->ready = ready;

    if (DEBUG_VIDEOS) {
        printf("video: init done (ready: %s)\n", ready ? "true":"false");
    }

    assert(texture);

    //set state
    playing = ready;
    paused = false;

    //send event

    //texture is ready
    texture->videoPlayerInitDone();

    if (ready) {
        //metadata available
        fireEvent("loadedmetadata");

        //playing
        fireEvent("playing");
    } else {
        //failed
        failed = true;
        fireEvent("error");
    }
}

/**
 * Video was rewinded.
 */
void AminoVideoPlayer::handleRewind() {
    fireEvent("rewind");
}

/**
 * Fire video player event.
 */
void AminoVideoPlayer::fireEvent(std::string event) {
    texture->fireVideoEvent(event);
}

//
// VideoDemuxer
//

/**
 * See http://dranger.com/ffmpeg/tutorial01.html.
 */
VideoDemuxer::VideoDemuxer() {
    //empty
}

VideoDemuxer::~VideoDemuxer() {
    //free resources
    close(true);
}

/**
 * Initialize the demuxer.
 */
bool VideoDemuxer::init() {
    //register all codecs and formats
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(58, 9, 100)
    av_register_all();
    avcodec_register_all();
#endif

    //support network calls
    avformat_network_init();

    //log level
    if (DEBUG_VIDEOS) {
        av_log_set_level(AV_LOG_INFO);
    } else {
        av_log_set_level(AV_LOG_QUIET);
    }

    if (DEBUG_VIDEOS) {
        printf("using libav %u.%u.%u\n", LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);

        //show hardware accelerated codecs
        /*
        printf("\n Hardware decoders:\n\n");

        //Note: deprecated (returns NULL)
        AVHWAccel *item = av_hwaccel_next(NULL);

        while (item) {
            printf(" -> %s\n", item->name);

            item = av_hwaccel_next(item);
        }

        printf("\n");
        */
    }

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

/**
 * Timeout callback. Returns true if timeout occured.
 */
static int interruptCallback(void *opaque) {
    VideoDemuxer *demuxer = static_cast<VideoDemuxer *>(opaque);

    if (demuxer->isTimeout()) {
        if (DEBUG_VIDEOS) {
            printf("-> timeout occured\n");
        }

        return 1;
    }

    return 0;
}

/**
 * Load a video from a file.
 */
bool VideoDemuxer::loadFile(std::string filename, std::string options) {
    //close previous instances
    close(false);

    this->filename = filename;
    this->options = options;

    if (DEBUG_VIDEOS) {
        printf("loading video: %s\n", filename.c_str());
    }

    //init
    const char *file = filename.c_str();

    context = avformat_alloc_context();

    if (!context) {
        lastError = "could not allocate context";
        close(false);

        return false;
    }

    //register timeout handler
    context->interrupt_callback.callback = interruptCallback;
    context->interrupt_callback.opaque = this;

    //test: disable UDP re-ordering
    //context->max_delay = 0;

    //options
    AVDictionary *opts = NULL;

    //av_dict_set(&opts, "rtsp_transport", "tcp", 0); //TCP instead of UDP (must be supported by server)
    //av_dict_set(&opts, "user_agent", "AminoGfx", 0);

    if (!options.empty()) {
        av_dict_parse_string(&opts, options.c_str(), "=", " ", 0);

        if (DEBUG_VIDEOS) {
            //show dictionary
            AVDictionaryEntry *t = NULL;

            printf("Options:\n");

            while ((t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
                printf(" %s: %s\n", t->key, t->value);
            }
        }
    }

    //check realtime
    AVDictionaryEntry *entry = av_dict_get(opts, "amino_realtime", NULL, AV_DICT_MATCH_CASE);

    if (entry) {
        //use setting
        realtime = strcmp(entry->value, "0") != 0 && strcmp(entry->value, "false") != 0;
    } else {
        //check RTSP
        realtime = filename.find("rtsp://") == 0;
    }

    //timeout settings
    entry = av_dict_get(opts, "amino_timeout_open", NULL, AV_DICT_MATCH_CASE);

    if (entry) {
        timeoutOpen = std::stoi(entry->value);
    }

    entry = av_dict_get(opts, "amino_timeout_read", NULL, AV_DICT_MATCH_CASE);

    if (entry) {
        timeoutRead = std::stoi(entry->value);
    }

    //dump format setting
    bool dumpFormat = false;

    entry = av_dict_get(opts, "amino_dump_format", NULL, AV_DICT_MATCH_CASE);

    if (entry) {
        //any value accepted
        dumpFormat = true;

        //change log level
        av_log_set_level(AV_LOG_INFO);
    }

    //open
    int res;

    resetTimeout(timeoutOpen);
    res = avformat_open_input(&context, file, NULL, &opts);

    av_dict_free(&opts);

    if (res < 0) {
        std::stringstream stream;

        stream << "file open error (-0x" << std::hex << res << ")";
        lastError = stream.str();
        close(false);

        return false;
    }

    //find stream
    if (avformat_find_stream_info(context, NULL) < 0) {
        lastError = "could not find streams";
        close(false);

        return false;
    }

    //debug
    if (DEBUG_VIDEOS || dumpFormat) {
        //output video format details
        av_dump_format(context, 0, file, 0);
    }

    //get video stream
    videoStream = av_find_best_stream(context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if (videoStream < 0) {
        videoStream = -1;
    }

    /* Note: previous implementation
    videoStream = -1;

    for (unsigned int i = 0; i < context->nb_streams; i++) {
        //Note: warning on macOS (codecpar not available on RPi)
        if (context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            //found video stream
            videoStream = i;

            //debug
            //printf(" -> found stream %i\n", i);

            break;
        }
    }
    */

    if (videoStream == -1) {
        lastError = "not a video";
        close(false);

        return false;
    }

    //calc duration
    stream = context->streams[videoStream];

    int64_t duration = stream->duration;

    durationSecs = duration * av_q2d(stream->time_base);

    if (durationSecs < 0) {
        durationSecs = -1;
    }

    //get properties
    codecCtx = stream->codec;

    //Note: not available in libav!
    /*
    if (codecCtx->framerate.num > 0 && codecCtx->framerate.den > 0) {
        fps = codecCtx->framerate.num / (float)codecCtx->framerate.den;
    }
    */

    if (codecCtx->time_base.num > 0 && codecCtx->time_base.den > 0) {
        fps = codecCtx->time_base.den / (float)codecCtx->time_base.num / codecCtx->ticks_per_frame;

        //debug
        //printf("framerate: %f\n", fps);
    }

    width = codecCtx->width;
    height = codecCtx->height;

    //check format
    isH264 = codecCtx->codec_id == AV_CODEC_ID_H264;
    isHEVC = codecCtx->codec_id == AV_CODEC_ID_HEVC;

    //debug
    if (DEBUG_VIDEOS) {
        printf("video found: duration=%i s, fps=%f, realtime=%s\n", (int)durationSecs, fps, realtime ? "true":"false");

        //Note: warning on macOS (codecpar not available on RPi)
        if (isH264) {
            printf(" -> H264\n");
        }

        if (isHEVC) {
            printf(" -> HEVC\n");
        }
    }

    return true;
}

#ifdef EGL_GBM

/**
 * @brief Get the Hw Format object.
 *
 * @param ctx
 * @param pix_fmts
 * @return enum AVPixelFormat
 */
static enum AVPixelFormat getHwFormat(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == AV_PIX_FMT_DRM_PRIME) {
            //debug cbxx
            printf(" -> found HW format\n");

            return *p;
        }
    }

    //debug cbxx
    printf("Failed to get HW surface format.\n");

    return AV_PIX_FMT_NONE;

    //DRM managed buffer
    //return AV_PIX_FMT_DRM_PRIME;

    //VA-PI
    //return AV_PIX_FMT_VAAPI;
}

#endif

/**
 * Initialize decoder.
 */
bool VideoDemuxer::initStream() {
    if (!codecCtx) {
        return false;
    }

    if (DEBUG_VIDEOS) {
        //show codec name
        char buf[1024];

        avcodec_string(buf, sizeof(buf), codecCtx, 0);

        printf(" -> codec: %s\n", buf);
    }

    //find decoder
    AVCodec *codec = NULL;

#ifdef EGL_GBM
    useV4L2 = false;
    useVaapi = false;

    if (codecCtx->codec_id == AV_CODEC_ID_H264) {
        /*
         * Default RPi 4 decoder:
         *
         *   - h264 (H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10)
         *     - works fine (min 12 fps)
         *
         * Hardware accelerated decoders:
         *
         *   - h264_v4l2m2m (V4L2 mem2mem H.264 decoder wrapper (codec h264))
         */

        //use V4L2
        codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
        useV4L2 = true;
        //useVaapi = true;

        if (!codec) {
            lastError = "could not find h264 v4l2m2m decoder";

            return false;
        }
    } else if (codecCtx->codec_id == AV_CODEC_ID_HEVC) {
        //supported: hevc_rpi hevc_v4l2m2m
        codec = avcodec_find_decoder_by_name("hevc_rpi");
        //cbxx FIXME could not initialize stream
    }
#endif

    //find the default decoder for the video stream
    if (!codec) {
        codec = avcodec_find_decoder(codecCtx->codec_id);
    }

    if (!codec) {
        lastError = "unsupported codec";

        return false;
    }

    if (DEBUG_VIDEOS) {
        printf(" -> decoder: %s (%s)\n", codec->name, codec->long_name);

        //show hardware configs
        //cbxx TODO
        /*
        if (codec->hw_configs) {
            int pos = 0;

            while (true) {
                AVCodecHWConfigInternal *item = (AVCodecHWConfigInternal *)codec->hw_configs[pos];

                if (!item) {
                    break;
                }

                printf("Hardware config:\n");

                //TODO item->public

                if (item->hwaccel) {
                    printf(" -> hardware accelerated\n");
                } else {
                    printf(" -> not hardware accelerated\n");
                }

                pos++;
            }
        }
        */
    }

    //copy context
    AVCodecContext *codecCtxOrig = codecCtx;

    codecCtx = avcodec_alloc_context3(codec);
    codecCtxAlloc = true;

    /* FIXME this replacement did not work (width and height were zero)
    if (avcodec_parameters_to_context(codecCtxOrig, stream->codecpar) < 0) {
        lastError = "could not copy codec context";

        return false;
    }
    */

    if (avcodec_copy_context(codecCtx, codecCtxOrig) != 0) {
        lastError = "could not copy codec context";

        return false;
    }

#ifdef EGL_GBM
    //init V2L2 (see https://github.com/jc-kynesim/hello_drmprime/blob/master/hello_drmprime.c)
    if (useV4L2) {
        const char * hwdev = "drm";
        enum AVHWDeviceType deviceType = av_hwdevice_find_type_by_name(hwdev);

        if (deviceType == AV_HWDEVICE_TYPE_NONE) {
            //output debug info
            fprintf(stderr, "Device type %s is not supported.\n", hwdev);
            fprintf(stderr, "Available device types:");

            while ((deviceType = av_hwdevice_iterate_types(deviceType)) != AV_HWDEVICE_TYPE_NONE) {
                fprintf(stderr, " %s", av_hwdevice_get_type_name(deviceType));
            }

            fprintf(stderr, "\n");

            //fail
            lastError = "could not initialize DRM decoder";

            return false;
        } else if (deviceType != AV_HWDEVICE_TYPE_DRM) {
            //wrong device type
            lastError = "DRM device not found";

            return false;
        }

        //debug cbxx
        printf("-> found DRM hardware device\n");
/*
        //open DRM device
        if ((drmFD = drmOpen("vc4", NULL)) < 0) {
            lastError = "could not open VC4 DRM device";

            return false;
        }

        //get DRM resources
        drmModeRes *res = drmModeGetResources(drmFD);

        if (!res) {
            lastError = "could not get VC4 resources";

            return false;
        }

        if (res->count_crtcs <= 0) {
            lastError = "no DRM crts found";

            drmModeFreeResources(res);

            return false;
        }

        //cbxx TODO move
        int conId = 0;
        uint32_t crtcId = 0;

        for (int i = 0; i < res->count_connectors; i++) {
            drmModeConnector *con = drmModeGetConnector(drmFD, res->connectors[i]);
            drmModeEncoder *enc = NULL;
            drmModeCrtc *crtc = NULL;

            if (con->encoder_id) {
                enc = drmModeGetEncoder(drmFD, con->encoder_id);

                if (enc->crtc_id) {
                    crtc = drmModeGetCrtc(drmFD, enc->crtc_id);
                }
            }

            if (!conId && crtc) {
                conId = con->connector_id;
                crtcId = crtc->crtc_id;
            }

            //debug
            fprintf(stderr, "Connector %d (crtc %d): type %d, %dx%d%s\n",
                   con->connector_id,
                   crtc ? crtc->crtc_id : 0,
                   con->connector_type,
                   crtc ? crtc->width : 0,
                   crtc ? crtc->height : 0,
                   (conId == (int)con->connector_id ?
                    " (chosen)" : ""));
        }

        if (!conId) {
            lastError = "no suitable enabled connector found";

            drmModeFreeResources(res);

            return false;
        }

        int crtcIdx = -1;

        for (int i = 0; i < res->count_crtcs; ++i) {
            if (crtcId == res->crtcs[i]) {
                crtcIdx = i;
                break;
            }
        }

        if (crtcIdx == -1) {
            lastError = "CRTC not found";

            drmModeFreeResources(res);

            return false;
        }

        if (res->count_connectors <= 0) {
            lastError = "no DRM connectors";

            drmModeFreeResources(res);

            return false;
        }

        drmModeConnector *c = drmModeGetConnector(drmFD, conId);

        if (!c) {
            lastError = "drmModeGetConnector failed";

            drmModeFreeResources(res);

            return false;
        }

        if (!c->count_modes) {
            lastError = "connector supports no mode";

            drmModeFreeConnector(c);
            drmModeFreeResources(res);

            return false;
        }
*/
        //cbxx needed?
        /*
        drmModeCrtc *crtc = drmModeGetCrtc(drmfd, s->crtcId);
        s->compose.x = crtc->x;
        s->compose.y = crtc->y;
        s->compose.width = crtc->width;
        s->compose.height = crtc->height;
        drmModeFreeCrtc(crtc);
        */

        //cbxx TODO more

        //debug cbxx
        printf("-> creating hardware decoder\n");

        //create the hardware decoder
        codecCtx->get_format = getHwFormat;
        codecCtx->hw_frames_ctx = NULL;
        codecCtx->hw_device_ctx = NULL; //cbxx TODO verify

        int err = 0;

        //cbxx type right? /dev/video10 ???
        //cbxx FIXME fails here -> bad address (EFAULT)
        //cbxx FIXME not defined -> https://ffmpeg.org/doxygen/2.3/error_8c.html#af516c6ccf78bd27740c438b30445272d
        if ((err = av_hwdevice_ctx_create(&codecCtx->hw_device_ctx, deviceType, NULL, NULL, 0)) < 0) {
            char str[1024] = { 0 };

            av_strerror(err, str, 1024);

            lastError = "could not initialize DRM device: " + std::to_string(err) + " " + std::string(str);

            return false;
        }

        codecCtx->thread_count = 3;
    }

    //init vaapi (see https://gist.github.com/kajott/d1b29c613be30893c855621edd1f212e#file-vaapi_egl_interop_example-c-L231)
    if (useVaapi) {
        std::string drmNode = "/dev/dri/renderD128";
        AVBufferRef *hwDeviceCtx = NULL;

        /*
         * FIXME VA-PI not supported on RPi 4:
         *
         *   - [AVHWDeviceContext @ 0xa38d03f0] libva: vaGetDriverNameByIndex() failed with unknown libva error, driver_name = (null)
         *   - [AVHWDeviceContext @ 0xa38d03f0] Failed to initialise VAAPI connection: -1 (unknown libva error).
         */
        if (av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_VAAPI, drmNode.c_str(), NULL, 0) < 0) {
            lastError = "could not initialize vaapi device";

            return false;
        }

        const AVHWDeviceContext *hwCtx = (AVHWDeviceContext*)hwDeviceCtx->data;
        const AVVAAPIDeviceContext *vaCtx = (AVVAAPIDeviceContext*)hwCtx->hwctx;

        vaDisplay = vaCtx->display;
        codecCtx->get_format = getHwFormat;
        codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
        codecCtx->thread_count = 3;
    }
#endif

    //open codec
    AVDictionary *opts = NULL;

    if (avcodec_open2(codecCtx, codec, &opts) < 0) {
        lastError = "could not open codec";

        return false;
    }

    if (DEBUG_VIDEOS) {
        //show HW acceleration
        printf(" -> hardware accelerated: %s\n", codecCtx->hwaccel ? "yes":"no");

        //show dictionary
        AVDictionaryEntry *t = NULL;

        while ((t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
            printf(" -> %s: %s\n", t->key, t->value);
        }
    }

    av_dict_free(&opts);

    //debug
    if (DEBUG_VIDEOS) {
        printf(" -> stream initialized\n");
    }

    return true;
}

/**
 * Check if NALU start codes are used (or format is annex b).
 */
bool VideoDemuxer::hasH264NaluStartCodes() {
    if (!context || !codecCtx || !isH264) {
        return false;
    }

    //Note: avcC atom  starts with 0x1
    return codecCtx->extradata_size < 7 || !codecCtx->extradata || *codecCtx->extradata != 0x1;
}

/**
 * Read the optional video header.
 */
bool VideoDemuxer::getHeader(uint8_t **data, int *size) {
    if (!context || !codecCtx) {
        return false;
    }

    if (codecCtx->extradata && codecCtx->extradata_size > 0) {
        *data = codecCtx->extradata;
        *size = codecCtx->extradata_size;

        return true;
    }

    return false;
}

/**
 * Read a video packet.
 *
 * Note: freeFrame() has to be called after the packet is consumed.
 */
READ_FRAME_RESULT VideoDemuxer::readFrame(AVPacket *packet) {
    if (!context || !codecCtx) {
        return READ_ERROR;
    }

    while (true) {
        //check state
        if (DEBUG_VIDEOS && paused) {
            printf("-> readFrame() while paused!\n");
        }

        //read
        int status;

        resetTimeout(timeoutRead);
        status = av_read_frame(context, packet);

        //check end of video
        if (status == AVERROR_EOF) {
            if (DEBUG_VIDEOS) {
                printf("-> end of file\n");
            }

            //FIXME getting EOF on RPi (libav) if animated gif is played!
            //cbxx FIXME getting this error on RPi 4 hardware decoder

            return READ_END_OF_VIDEO;
        }

        //check error
        if (status < 0) {
            if (DEBUG_VIDEOS) {
                printf("-> read frame error: %i\n", status);
            }

            return READ_ERROR;
        }

        //is this a packet from the video stream?
        if (packet->stream_index == videoStream) {
            return READ_OK;
        }

        //process next frame
        av_free_packet(packet);
    }
}

/**
 * Get the presentation time (in seconds).
 */
double VideoDemuxer::getFramePts(AVPacket *packet) {
    if (!stream || !packet) {
        return 0;
    }

    if (packet->pts != (int64_t)AV_NOPTS_VALUE) {
        return packet->pts * av_q2d(stream->time_base);
    }

    return 0;
}

/**
 * Free a video packet.
 */
void VideoDemuxer::freeFrame(AVPacket *packet) {
    //free the packet that was allocated by av_read_frame
    av_free_packet(packet);

    packet->data = NULL;
    packet->size = 0;
}

/**
 * Read a decoded video frame.
 */
READ_FRAME_RESULT VideoDemuxer::readDecodedFrame(double &time) {
    if (!context || !codecCtx) {
        return READ_ERROR;
    }

    //initialize
    if (!frame) {
        //allocate video frame
        frame = av_frame_alloc();

        //init RGB
        if (!useVaapi && !useV4L2) {
            //allocate an AVFrame structure
            frameRGB = av_frame_alloc();

            if (!frame || !frameRGB) {
                lastError = "could not allocate frame";
                return READ_ERROR;
            }

            if (!buffer) {
                //determine required buffer size and allocate buffer (using RGB 24 bits)
                //Note: deprecated warning on macOS
                int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height);
                //int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height, 1);

                //debug
                //printf("-> picture size: %i (%ix%i)\n", numBytes, codecCtx->width, codecCtx->height);

                bufferSize = numBytes * sizeof(uint8_t);
                buffer = (uint8_t *)av_malloc(bufferSize);

                assert(buffer);

                memset(buffer, 0, bufferSize);

                if (!bufferCurrent) {
                    bufferCurrent = (uint8_t *)av_malloc(bufferSize);
                }
            }

            //fill buffer
            //Note: deprecated warning on macOS

            avpicture_fill((AVPicture *)frameRGB, buffer, AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height);
            //av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height, 1);

            switchDecodedFrame();

            //initialize SWS context for software scaling
            sws_ctx = sws_getContext(codecCtx->width, codecCtx->height, codecCtx->pix_fmt, codecCtx->width, codecCtx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
        }
    }

    //read frame
    AVPacket *packet = av_packet_alloc();
    READ_FRAME_RESULT res;

    assert(packet);

    packet->data = NULL;
    packet->size = 0;

    //cbxx TODO better packet memory management
    while (true) {
        res = readFrame(packet);

        if (res == READ_OK) {
#ifdef EGL_GBM
            //V4L2
            if (useV4L2) {
                //send to hardware decoder
                if (avcodec_send_packet(codecCtx, packet) < 0) {
                    res = READ_ERROR;
                    goto done;
                }

                //get from decoder
                int ret = avcodec_receive_frame(codecCtx, frame);

                if (ret == AVERROR(EAGAIN)) {
                    //next frame
                    freeFrame(packet);
                    continue;
                }

                if (ret == AVERROR_EOF) {
                    ret = READ_END_OF_VIDEO;
                    goto done;
                } else if (ret < 0) {
                    ret = READ_ERROR;
                    goto done;
                }

                //process
                //cbxx TODO drmprime_out_display(dpo, frame);
                goto done;
            }

            //vaapi
            if (useVaapi) {
                //cbxx TODO verify
                //send to hardware decoder
                if (avcodec_send_packet(codecCtx, packet) < 0) {
                    res = READ_ERROR;
                    goto done;
                }

                //get from decoder
                int ret = avcodec_receive_frame(codecCtx, frame);

                if (ret == AVERROR(EAGAIN)) {
                    //next frame
                    freeFrame(packet);
                    continue;
                }

                if (ret == AVERROR_EOF) {
                    ret = READ_END_OF_VIDEO;
                    goto done;
                } else if (ret < 0) {
                    ret = READ_ERROR;
                    goto done;
                }

                //process
                VASurfaceID vaSurface = (uintptr_t)frame->data[3];
                VADRMPRIMESurfaceDescriptor prime;

                if (vaExportSurfaceHandle(vaDisplay, vaSurface, VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2, VA_EXPORT_SURFACE_READ_ONLY | VA_EXPORT_SURFACE_SEPARATE_LAYERS, &prime) != VA_STATUS_SUCCESS) {
                    ret = READ_ERROR;
                    goto done;
                }

                if (prime.fourcc != VA_FOURCC_NV12) {
                    ret = READ_ERROR;
                    goto done;
                }

                vaSyncSurface(vaDisplay, vaSurface);

                //cbxx TODO debug

                goto done;
            }
#endif

            //use RGB decoder

            //decode video frame
            int frameFinished;

            avcodec_decode_video2(codecCtx, frame, &frameFinished, packet);

            //did we get a video frame?
            if (frameFinished) {
                //convert the image from its native format to RGB
                sws_scale(sws_ctx, (uint8_t const * const *)frame->data, frame->linesize, 0, codecCtx->height, frameRGB->data, frameRGB->linesize);

                //frameRGB is ready
                frameRGBCount++;

                //timing
                double pts;

                if (packet->dts != (int64_t)AV_NOPTS_VALUE) {
#ifdef MAC
                    pts = av_frame_get_best_effort_timestamp(frame) * av_q2d(stream->time_base);
#else
                    //fallback (currently not yet used)
                    if (frame->pts != (int64_t)AV_NOPTS_VALUE) {
                        pts = frame->pts * av_q2d(stream->time_base);
                    } else {
                        pts = 0;
                    }
#endif
                } else {
                    pts = 0;
                }

                if (pts != 0) {
                    //store last value
                    lastPts = pts;
                } else {
                    //use last value
                    pts = lastPts;

                    if (DEBUG_VIDEO_FRAMES) {
                        printf("-> using internal timer\n");
                    }
                }

                //calc next value
                double frameDelay = av_q2d(stream->codec->time_base);

                frameDelay += frame->repeat_pict * (frameDelay * .5); //support repeating frames
                lastPts += frameDelay;

                time = pts;

                //debug
                if (DEBUG_VIDEO_FRAMES) {
                    printf("frame read: time=%f s\n", pts);
                }

                goto done;
            }

            //next frame
            freeFrame(packet);
            continue;
        }

        //error case

done:
        freeFrame(packet);
        av_packet_free(&packet);
        break;
    }

    return res;
}

#pragma GCC diagnostic pop

/**
 * Switch active RGB frame.
 */
void VideoDemuxer::switchDecodedFrame() {
#ifdef EGL_GBM
    if (useVaapi) {
        //cbxx TODO vaapi
        return;
    }
#endif

    //RGB decoder
    //cbxx TODO optimize buffer copy
    memcpy(bufferCurrent, buffer, bufferSize);
    bufferCount = frameRGBCount;
}

/**
 * Pause reading frames.
 */
void VideoDemuxer::pause() {
    if (paused) {
        return;
    }

    paused = true;

    if (context) {
        if (DEBUG_VIDEOS) {
            printf("pausing stream\n");
        }

        av_read_pause(context);
    }
}

/**
 * Resume reading frames.
 */
void VideoDemuxer::resume() {
    if (!paused) {
        return;
    }

    paused = false;

    if (context) {
        if (DEBUG_VIDEOS) {
            printf("resuming stream\n");
        }

        av_read_play(context);
    }
}

/**
 * Rewind playback (go back to first frame).
 */
bool VideoDemuxer::rewind() {
    //reload & verify
    size_t prevW = width;
    size_t prevH = height;

    if (!loadFile(filename, options) || prevW != width || prevH != height) {
        return false;
    }

    //prepare stream
    return initStream();
}

/**
 * Rewind RGB stream.
 */
bool VideoDemuxer::rewindDecoder(double &time) {
    if (!rewind()) {
        return false;
    }

    //load first frame
    return readDecodedFrame(time) == READ_OK;
}

/**
 * Get frame data.
 */
uint8_t *VideoDemuxer::getFrameRGBData(int &id) {
    id = bufferCount;

    return bufferCurrent;
}

/**
 * Close handlers.
 */
void VideoDemuxer::close(bool destroy) {
    if (context) {
        avformat_close_input(&context);
        context = NULL;
    }

    if (codecCtx && codecCtxAlloc) {
        avcodec_free_context(&codecCtx);
        codecCtx = NULL;
        codecCtxAlloc = false;
    }

    videoStream = -1;
    stream = NULL;

    durationSecs = -1;
    fps = -1;
    width = 0;
    height = 0;
    isH264 = false;
    isHEVC = false;

    //close read
    closeReadFrame(destroy);
}

/**
 * Free readFrame() resources.
 */
void VideoDemuxer::closeReadFrame(bool destroy) {
    if (frame) {
        av_frame_free(&frame);
        frame = NULL;
    }

    if (frameRGB) {
        av_frame_free(&frameRGB);
        frameRGB = NULL;
    }

    frameRGBCount = -1;
    lastPts = 0;

    if (buffer) {
        av_free(buffer);
        buffer = NULL;
    }

    //Note: kept until demuxer is destroyed
    if (destroy && bufferCurrent) {
        av_free(bufferCurrent);
        bufferCurrent = NULL;
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = NULL;
    }
}

/**
 * Set timeout.
 */
void VideoDemuxer::resetTimeout(int timeoutMS) {
    timeout = getTime() + timeoutMS;
}

/**
 * Check if timeout occured.
 */
bool VideoDemuxer::isTimeout() {
    return getTime() > timeout;
}

/**
 * Get the last error.
 */
std::string VideoDemuxer::getLastError() {
    return lastError;
}

//
// AnyVideoStream
//

AnyVideoStream::AnyVideoStream() {
    //empty
}

AnyVideoStream::~AnyVideoStream() {
    //empty
}

/**
 * Get last error.
 */
std::string AnyVideoStream::getLastError() {
    return lastError;
}

//
// VideoFileStream
//

VideoFileStream::VideoFileStream(std::string filename, std::string options): AnyVideoStream(), filename(filename), options(options) {
    if (DEBUG_VIDEOS) {
        printf("create video file stream\n");
    }

    //empty
}

VideoFileStream::~VideoFileStream() {
    if (file) {
        fclose(file);
        file = NULL;
    }

    if (hasPacket) {
        demuxer->freeFrame(packet);
        hasPacket = false;

        av_packet_free(&packet);
        packet = nullptr;
    }

    if (demuxer) {
        delete demuxer;
        demuxer = NULL;
    }
}

/**
 * Open the file.
 */
bool VideoFileStream::init() {
    if (DEBUG_VIDEOS) {
        printf("-> init video file stream\n");
    }

    //Note: always using the demuxer on Pi 4
#ifndef EGL_GBM
    //OMX decoder only
    //check local H264 (direct file access)
    std::string postfix = ".h264";
    bool isLocalH264 = false;

    if (filename.compare(filename.length() - postfix.length(), postfix.length(), postfix) == 0 && filename.find("://") == std::string::npos) {
        isLocalH264 = true;
    }

    if (isLocalH264) {
        //local file
        file = fopen(filename.c_str(), "rb");

        if (!file) {
            lastError = "file not found";

            return false;
        }

        if (DEBUG_VIDEOS) {
            printf("-> local H264 file\n");
        }
    }
#endif

    if (!file) {
        //any stream

        //init demuxer
        demuxer = new VideoDemuxer();

        if (!demuxer->init() || !demuxer->loadFile(filename, options) || !demuxer->initStream()) {
            lastError = demuxer->getLastError();

            return false;
        }

        //init packet
        assert(!packet);

        packet = av_packet_alloc();

        assert(packet);

        packet->data = NULL;
        packet->size = 0;

        if (DEBUG_VIDEOS) {
            printf("-> video stream\n");
        }
    }

    return true;
}

/**
 * Rewind the file stream.
 */
bool VideoFileStream::rewind() {
    if (DEBUG_VIDEOS) {
        printf("-> rewind video file stream\n");
    }

    if (file) {
        //file
        std::rewind(file);
    } else if (demuxer) {
        //stream
        if (hasPacket) {
            demuxer->freeFrame(packet);
            hasPacket = false;
        }

        eof = false;
        failed = false;
        headerRead = false;

        //rewind stream
        return demuxer->rewind();
    }

    return true;
}

/**
 * Read from the stream.
 */
unsigned int VideoFileStream::read(unsigned char *dest, unsigned int length, omx_metadata_t &omxData) {
    //reset OMX data
    omxData.flags = 0;
    omxData.timeStamp = 0;

    if (file) {
        //debug cbxx -> not called
        printf("-> read file\n");

        //read block from file (Note: ferror() returns error state)
        return fread(dest, 1, length, file);
    } else if (demuxer) {
        //debug cbxx -> check OMX special -> not called
        printf("-> read stream\n");

        //header
        if (!headerRead) {
            uint8_t *data;
            int size;

            if (!demuxer->getHeader(&data, &size)) {
                //no header available
                headerRead = true;
            }

            //fill
            unsigned int dataLeft = size - packetOffset;
            unsigned int dataLen = std::min(dataLeft, length);

            if (DEBUG_VIDEO_STREAM) {
                printf("-> writing header: %i\n", dataLen);
            }

            memcpy(dest, data + packetOffset, dataLen);

            //check packet
            if (dataLeft == dataLen) {
                //all consumed
                packetOffset = 0;
                headerRead = true;
                omxData.flags |= 0x80; //OMX_BUFFERFLAG_CODECCONFIG
            } else {
                //data left
                packetOffset += dataLen;
            }

            return dataLen;
        }

        //check existing packet
        unsigned int offset = 0;

        if (hasPacket) {
            unsigned int dataLeft = packet->size - packetOffset;
            unsigned int dataLen = std::min(dataLeft, length);

            if (DEBUG_VIDEO_STREAM) {
                printf("-> filling read buffer (prev packet): %i of %i\n", dataLen, length);
            }

            memcpy(dest, packet->data + packetOffset, dataLen);
            offset += dataLen;

            //check packet
            if (dataLeft == dataLen) {
                //all consumed
                demuxer->freeFrame(packet);
                hasPacket = false;
                packetOffset = 0;
            } else {
                //data left
                packetOffset += dataLen;
                return offset;
            }

            //check buffer
            if (offset == length) {
                //buffer full
                return offset;
            }

            //read next packet
        }

        //check EOF
        if (eof || failed) {
            return offset;
        }

        //read new packet
        READ_FRAME_RESULT res = demuxer->readFrame(packet);

        if (res == READ_END_OF_VIDEO) {
            eof = true;
            demuxer->freeFrame(packet);

            return offset;
        }

        if (res != READ_OK) {
            failed = true;
            demuxer->freeFrame(packet);

            return offset;
        }

        //fill buffer
        hasPacket = true;

        unsigned int dataLeft = length - offset;
        unsigned int dataLen = std::min(dataLeft, (unsigned int)packet->size);

        if (DEBUG_VIDEO_STREAM) {
            printf("-> filling read buffer (new packet): %i of %i\n", dataLen, dataLeft);
        }

        memcpy(dest + offset, packet->data, dataLen);
        offset += dataLen;

        //timing
        omxData.timeStamp = demuxer->getFramePts(packet) * 1000000;

        //check state
        if (dataLen < (unsigned int)packet->size) {
            //data left
            packetOffset = dataLen;
        } else {
            //all consumed
            demuxer->freeFrame(packet);
            hasPacket = false;
            omxData.flags |= 0x10; //OMX_BUFFERFLAG_ENDOFFRAME
        }

        return offset;
    }

    return 0;
}

/**
 * Pause reading.
 */
void VideoFileStream::pause() {
    if (demuxer) {
        demuxer->pause();
    }
}

/**
 * Resume reading.
 */
void VideoFileStream::resume() {
    if (demuxer) {
        demuxer->resume();
    }
}

/**
 * Check end of stream.
 */
bool VideoFileStream::endOfStream() {
    if (file) {
        return feof(file);
    } else if (demuxer) {
        return eof;
    }

    assert(false);

    return true;
}

/**
 * Check if H264 video stream.
 */
bool VideoFileStream::isH264() {
    //Note: file is always raw H264
    return file || (demuxer && demuxer->isH264);
}

/**
 * Check if HEVC video stream.
 */
bool VideoFileStream::isHEVC() {
    return demuxer && demuxer->isHEVC;
}

/**
 * Check if NALU start codes are used (or annex b).
 */
bool VideoFileStream::hasH264NaluStartCodes() {
    return demuxer && demuxer->hasH264NaluStartCodes();
}

/**
 * Get playback duration.
 */
double VideoFileStream::getDuration() {
    if (demuxer) {
        return demuxer->durationSecs;
    }

    return -1;
}

/**
 * Get the framerate (0 if unknown).
 */
double VideoFileStream::getFramerate() {
    if (demuxer) {
        return demuxer->fps;
    }

    return 0;
}

/**
 * Get demuxer instance.
 */
VideoDemuxer* VideoFileStream::getDemuxer() {
    return demuxer;
}