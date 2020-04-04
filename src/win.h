#ifndef _AMINO_WIN_H
#define _AMINO_WIN_H

#include "base.h"
#include "renderer.h"

/**
 * AminoGfxMac factory.
 */
class AminoGfxWinFactory : public AminoJSObjectFactory {
public:
    AminoGfxWinFactory(Nan::FunctionCallback callback);

    AminoJSObject* create() override;
};

/**
 * Mac video player.
 */
class AminoWinVideoPlayer : public AminoVideoPlayer {
public:
    AminoWinVideoPlayer(AminoTexture *texture, AminoVideo *video);
    ~AminoWinVideoPlayer();

    bool initStream() override;
    void init() override;
    void initVideoTexture() override;
    void updateVideoTexture(GLContext *ctx) override;
    bool initTexture();

    //metadata
    double getMediaTime() override;
    double getDuration() override;
    double getFramerate() override;
    void stopPlayback() override;
    bool pausePlayback() override;
    bool resumePlayback() override;

private:
    std::string filename;
    std::string options;
    VideoDemuxer *demuxer = NULL;
    int frameId = -1;
    uv_mutex_t frameLock;

    uv_thread_t thread;
    bool threadRunning = false;

    double mediaTime = -1;
    bool doStop = false;
    bool doPause = false;
    uv_sem_t pauseSem;

    void initDemuxer();
    void closeDemuxer();
    static void demuxerThread(void *arg);
};

#endif
