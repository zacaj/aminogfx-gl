#ifndef _AMINOVIDEOS_H
#define _AMINOVIDEOS_H

#include "base_js.h"
#include "gfx.h"

// extern "C" {
//     #include "libavcodec/avcodec.h"
//     #include "libavformat/avformat.h"
//     #include "libswscale/swscale.h"
// }

//cbxx FIXME
#define DEBUG_VIDEOS true

class AminoVideoFactory;
class AminoTexture;
class GLContext;

/**
 * Amino Video Loader.
 *
 */
class AminoVideo : public AminoJSObject {
public:
    AminoVideo();
    ~AminoVideo();

    bool getPlaybackLoop(int32_t &loop);
    std::string getPlaybackSource();
    std::string getPlaybackOptions();

    //creation
    static AminoVideoFactory* getFactory();

    //init
    static NAN_MODULE_INIT(Init);

private:
    //JS constructor
    static NAN_METHOD(New);
};

/**
 * AminoImage class factory.
 */
class AminoVideoFactory : public AminoJSObjectFactory {
public:
    AminoVideoFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Amino Video Player.
 */
class AminoVideoPlayer {
public:
    AminoVideoPlayer(AminoTexture *texture, AminoVideo *video);
    virtual ~AminoVideoPlayer();

    virtual bool initStream() = 0; //called on main thread
    virtual void init() = 0; //called on OpenGL thread
    virtual int getNeededTextures();
    virtual void initVideoTexture() = 0;
    virtual void updateVideoTexture(GLContext *ctx) = 0;
    virtual void destroy();
    void destroyAminoVideoPlayer();

    bool isReady();
    bool isPlaying();
    bool isPaused();

    std::string getLastError();

    //metadata
    void getVideoDimension(int32_t &w, int32_t &h);
    virtual double getMediaTime() = 0;
    virtual double getDuration() = 0;
    virtual double getFramerate() = 0;
    std::string getState();
    virtual void stopPlayback() = 0;
    virtual bool pausePlayback() = 0;
    virtual bool resumePlayback() = 0;

protected:
    AminoTexture *texture;
    AminoVideo *video;

    //state
    bool initDone = false;
    bool ready = false;
    bool playing = false;
    bool paused = false;

    bool failed = false;
    bool destroyed = false;
    std::string lastError;

    //settings
    int32_t loop = -1;

    //video
    int32_t videoW = 0;
    int32_t videoH = 0;

    void handlePlaybackDone();
    void handlePlaybackError();
    void handlePlaybackStopped();
    void handlePlaybackPaused();
    void handlePlaybackResumed();

    void handleInitDone(bool ready);

    void handleRewind();

    void fireEvent(std::string event);
};

enum READ_FRAME_RESULT {
    READ_OK = 0,
    READ_ERROR,
    READ_END_OF_VIDEO
};

/**
 * Demux a video container stream.
 */
class VideoDemuxer {
public:
    size_t width = 0;
    size_t height = 0;
    float fps = -1;
    float durationSecs = -1.f;
    bool isH264 = false;
    bool isHEVC = false;
    bool realtime = false;

    VideoDemuxer();
    virtual ~VideoDemuxer();

    bool init();
    bool loadFile(std::string filename, std::string options);
    bool initStream();

    READ_FRAME_RESULT readRGBFrame(double &time);
    void switchRGBFrame();

    bool hasH264NaluStartCodes();
    bool getHeader(uint8_t **data, int *size);
    READ_FRAME_RESULT readFrame(int *packet);
    double getFramePts(int *packet);
    void freeFrame(int *packet);

    void pause();
    void resume();

    bool rewind();
    bool rewindRGB(double &time);
    uint8_t *getFrameData(int &id);

    bool isTimeout();

    std::string getLastError();

private:
    std::string lastError;

    //context
    int *context = NULL;
    int *codecCtx = NULL;
    bool codecCtxAlloc = false;

    //stream info
    std::string filename;
    std::string options;
    int videoStream = -1;
    int *stream = NULL;

    //timeout
    double timeout = 0;
    int timeoutOpen = 5000; //5s
    int timeoutRead = 1000; //1s

    //read
    int *frame = NULL;
    int *frameRGB = NULL;
    int frameRGBCount = -1;
    double lastPts = 0;
    uint8_t *buffer = NULL;
    uint8_t *bufferCurrent = NULL;
    int bufferCount = -1;
    unsigned int bufferSize = 0;
    bool paused = false;

    struct SwsContext *sws_ctx = NULL;

    void close(bool destroy);
    void closeReadFrame(bool destroy);

    void resetTimeout(int timeoutMS);
};

struct omx_metadata_t {
    unsigned int flags;
    signed long long timeStamp;
};

/**
 * Abstract video stream.
 */
class AnyVideoStream {
public:
    AnyVideoStream();
    virtual ~AnyVideoStream();

    virtual bool init() = 0;

    virtual bool endOfStream() = 0;
    virtual bool rewind() = 0;

    virtual unsigned int read(unsigned char *dest, unsigned int length, omx_metadata_t &omxData) = 0;

    virtual void pause() = 0;
    virtual void resume() = 0;

    //metadata
    virtual bool isH264() = 0;
    virtual bool isHEVC() = 0;
    virtual bool hasH264NaluStartCodes() = 0;
    virtual double getDuration() = 0;
    virtual double getFramerate() = 0;

    virtual VideoDemuxer* getDemuxer() = 0;

    std::string getLastError();

protected:
    std::string lastError;
};

/**
 * Video file stream.
 */
class VideoFileStream : public AnyVideoStream {
public:
    VideoFileStream(std::string filename, std::string options);
    ~VideoFileStream();

    bool init() override;

    bool endOfStream() override;
    bool rewind() override;

    unsigned int read(unsigned char *dest, unsigned int length, omx_metadata_t &omxData) override;

    void pause() override;
    void resume() override;

    //metadata
    bool isH264() override;
    bool isHEVC() override;
    bool hasH264NaluStartCodes() override;
    double getDuration() override;
    double getFramerate() override;

    VideoDemuxer* getDemuxer() override;

private:
    std::string filename;
    std::string options;

    //file
    FILE *file = NULL;

    //demuxer
    VideoDemuxer *demuxer = NULL;
    int packet;
    bool hasPacket = false;
    bool eof = false;
    bool failed = false;
    unsigned int packetOffset = 0;
    bool headerRead = false;
};

#endif