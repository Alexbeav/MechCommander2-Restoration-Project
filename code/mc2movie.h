#ifndef MC2MOVIE_H
#define MC2MOVIE_H

// MC2Movie — Video playback decoded by FFmpeg into a gos-owned texture.
// Decode/audio machinery cloned from MP4Player; rendering is done by the
// caller via the standard gos UI pipeline (gos_SetRenderState +
// gos_DrawQuads against getTextureHandle()).
//
// See FMV_DESIGN.md §2 for the public API and §4 for texture lifecycle.

#include <string>
#include <queue>
#include <mutex>
#include <atomic>
#include <SDL2/SDL.h>
#include "platform_windows.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

class MC2Movie {
public:
    MC2Movie();
    ~MC2Movie();

    bool init(const char* path, RECT rect, bool loop);
    void update();
    bool isPlaying() const;
    void stop();
    void restart();
    const std::string& getMovieName() const;
    void setRect(RECT rect);
    DWORD getTextureHandle() const;

private:
    void openFile(const std::string& path);
    void cleanup();
    void demux();
    void decodeVideo();

    static void audioCallback(void* userdata, Uint8* stream, int len);
    double getClock();

    // FFmpeg demux/decode
    AVFormatContext* fmtCtx;
    AVCodecContext* vidCtx;
    AVCodecContext* audCtx;
    AVFrame* decFrame;
    AVFrame* rgbFrame;
    uint8_t* rgbBuf;
    int rgbBufSize;
    SwsContext* swsCtx;
    SwrContext* swrCtx;
    int vidIdx, audIdx;
    double vidTimeBase, audTimeBase;
    double frameDur;
    int vidW, vidH;
    double frameRate;

    // Video packet queue
    std::queue<AVPacket*> vidPktQueue;

    // Audio PCM queue
    struct AudioChunk {
        uint8_t* data;
        int size;
        int offset;
        double pts;
    };
    std::queue<AudioChunk> audPcmQueue;
    std::mutex audioMutex;
    AVFrame* audFrame;

    // SDL audio
    SDL_AudioDeviceID audioDevice;
    SDL_AudioSpec audioSpec;
    std::atomic<double> audioClock{0.0};
    int sampleRate, channels;
    bool hasAudio;
    bool audioStarted;

    // gos-owned frame texture (replaces MP4Player's private GLuint).
    DWORD gosTextureHandle;
    bool textureReady;

    // Pending video frame (decoded, waiting for PTS)
    uint8_t* pendingData;
    double pendingPts;
    bool hasPending;

    // Playback state
    RECT displayRect;
    std::string moviePath;
    bool playing;
    bool looped;
    bool demuxEof;
    bool clockRunning;
    int frameCount;

    // Wall clock fallback
    Uint64 perfFreq, startCounter;
};

#endif
