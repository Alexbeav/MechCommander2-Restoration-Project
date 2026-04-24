#ifndef FRAME_RATE_FIX_H
#define FRAME_RATE_FIX_H

#include <chrono>

// Frame rate timing fixes for different video types
class FrameRateFixer {
public:
    // Get correct frame timing for different video types
    static double getCorrectFrameTime(double detectedFps);

    // Get dynamic frame timing based on video content
    static double getDynamicFrameTime(double videoFps, const std::string& filename);

    // Check if video needs frame rate timing adjustment
    static bool needsTimingFix(double fps);

    // Get frame rate category
    enum class FrameRateCategory {
        CINEMA_24FPS,    // 24 fps videos (MSFT.mp4)
        GAME_15FPS,      // 15 fps videos (CINEMA1, pilots, briefings)
        STANDARD_30FPS,  // 30 fps videos
        OTHER
    };

    static FrameRateCategory categorizeFrameRate(double fps);

    // High-precision timing for smooth playback
    class PrecisionTimer {
    public:
        PrecisionTimer(double targetFps);
        bool shouldRenderFrame();
        void markFrameRendered();
        double getActualFps() const;
        void reset();

    private:
        double targetFrameTime;
        std::chrono::high_resolution_clock::time_point lastFrameTime;
        std::chrono::high_resolution_clock::time_point startTime;
        int frameCount;
        double accumelatedError;
    };

private:
    static double adjustForVideoType(double baseFps, const std::string& filename);
};

#endif