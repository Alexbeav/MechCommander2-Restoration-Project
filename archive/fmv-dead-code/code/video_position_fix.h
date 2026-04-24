#ifndef VIDEO_POSITION_FIX_H
#define VIDEO_POSITION_FIX_H

#include <windows.h>
#include <string>

// Video type detection and positioning fixes
enum class VideoType {
    INTRO_FULLSCREEN,    // MSFT.mp4, CINEMA1-4.mp4 in fullscreen mode
    BRIEFING_WINDOW,     // Briefing videos in game UI window
    PILOT_AVATAR,        // Tiny pilot videos (76x54)
    CUTSCENE_FULLSCREEN, // Mission cutscenes
    UNKNOWN
};

struct VideoInfo {
    int width;
    int height;
    double fps;
    VideoType type;
    RECT recommendedRect;
    bool needsScaling;
    float minScaleFactor;
};

class VideoPositionFixer {
public:
    // Detect video type and get optimal positioning
    static VideoInfo analyzeVideo(const std::string& filename, int videoWidth, int videoHeight, double fps);

    // Get corrected rectangle for different video types
    static RECT getCorrectRect(VideoType type, int videoWidth, int videoHeight, int windowWidth, int windowHeight);

    // Get corrected rectangle for briefing videos (game UI integration)
    static RECT getBriefingRect(int windowWidth, int windowHeight);

    // Get safe scaling for tiny videos
    static RECT getSafeScaledRect(int videoWidth, int videoHeight, int windowWidth, int windowHeight, int minSize = 150);

    // Check if video needs special handling
    static bool needsPositionFix(int videoWidth, int videoHeight);

private:
    static VideoType detectVideoType(const std::string& filename, int width, int height);
    static bool isIntroVideo(const std::string& filename);
    static bool isPilotVideo(const std::string& filename);
    static bool isBriefingVideo(const std::string& filename);
};

#endif