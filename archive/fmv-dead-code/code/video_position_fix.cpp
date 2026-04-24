#include "video_position_fix.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <string>

VideoInfo VideoPositionFixer::analyzeVideo(const std::string& filename, int videoWidth, int videoHeight, double fps) {
    VideoInfo info;
    info.width = videoWidth;
    info.height = videoHeight;
    info.fps = fps;
    info.type = detectVideoType(filename, videoWidth, videoHeight);
    info.needsScaling = false;
    info.minScaleFactor = 1.0f;

    std::cout << "[VideoPositionFixer] Analyzing: " << filename
              << " (" << videoWidth << "x" << videoHeight << ", " << fps << "fps)" << std::endl;

    // Determine recommended rectangle based on video type and size
    switch (info.type) {
        case VideoType::INTRO_FULLSCREEN:
        case VideoType::CUTSCENE_FULLSCREEN:
            // Fullscreen videos - use entire window
            info.recommendedRect = {0, 0, 1920, 1080}; // Will be adjusted to actual window size
            std::cout << "[VideoPositionFixer] → FULLSCREEN video" << std::endl;
            break;

        case VideoType::BRIEFING_WINDOW:
            // Briefing videos should go in the briefing UI area, not fullscreen
            info.recommendedRect = getBriefingRect(1920, 1080);
            std::cout << "[VideoPositionFixer] → BRIEFING video (UI window)" << std::endl;
            break;

        case VideoType::PILOT_AVATAR:
            // Tiny pilot videos need safe scaling
            info.needsScaling = true;
            info.minScaleFactor = 150.0f / (videoWidth > videoHeight ? videoWidth : videoHeight); // Scale to at least 150px
            info.recommendedRect = getSafeScaledRect(videoWidth, videoHeight, 1920, 1080);
            std::cout << "[VideoPositionFixer] → PILOT avatar (safe scaling needed)" << std::endl;
            break;

        default:
            // Default positioning
            info.recommendedRect = {0, 0, videoWidth, videoHeight};
            std::cout << "[VideoPositionFixer] → UNKNOWN type (default positioning)" << std::endl;
            break;
    }

    return info;
}

RECT VideoPositionFixer::getCorrectRect(VideoType type, int videoWidth, int videoHeight, int windowWidth, int windowHeight) {
    switch (type) {
        case VideoType::INTRO_FULLSCREEN:
        case VideoType::CUTSCENE_FULLSCREEN:
            return {0, 0, windowWidth, windowHeight};

        case VideoType::BRIEFING_WINDOW:
            return getBriefingRect(windowWidth, windowHeight);

        case VideoType::PILOT_AVATAR:
            return getSafeScaledRect(videoWidth, videoHeight, windowWidth, windowHeight);

        default:
            return {0, 0, videoWidth, videoHeight};
    }
}

RECT VideoPositionFixer::getBriefingRect(int windowWidth, int windowHeight) {
    // Typical briefing video area in MechCommander 2 UI
    // These coordinates should match your actual UI layout

    // Scale based on window size (assuming 1920x1080 base)
    float scaleX = windowWidth / 1920.0f;
    float scaleY = windowHeight / 1080.0f;

    // Briefing area (approximate - adjust based on your UI)
    int briefingX = static_cast<int>(300 * scaleX);      // Left offset from edge
    int briefingY = static_cast<int>(150 * scaleY);      // Top offset from edge
    int briefingWidth = static_cast<int>(800 * scaleX);  // Width of briefing area
    int briefingHeight = static_cast<int>(600 * scaleY); // Height of briefing area

    RECT rect = {briefingX, briefingY, briefingX + briefingWidth, briefingY + briefingHeight};

    std::cout << "[VideoPositionFixer] Briefing rect: " << rect.left << "," << rect.top
              << " " << (rect.right - rect.left) << "x" << (rect.bottom - rect.top) << std::endl;

    return rect;
}

RECT VideoPositionFixer::getSafeScaledRect(int videoWidth, int videoHeight, int windowWidth, int windowHeight, int minSize) {
    // Ensure video is at least minSize pixels in largest dimension
    int currentMax = (videoWidth > videoHeight ? videoWidth : videoHeight);

    if (currentMax < minSize) {
        float scaleFactor = static_cast<float>(minSize) / currentMax;
        videoWidth = static_cast<int>(videoWidth * scaleFactor);
        videoHeight = static_cast<int>(videoHeight * scaleFactor);

        std::cout << "[VideoPositionFixer] Scaling tiny video by " << scaleFactor
                  << " to " << videoWidth << "x" << videoHeight << std::endl;
    }

    // Center the video in the window
    int x = (windowWidth - videoWidth) / 2;
    int y = (windowHeight - videoHeight) / 2;

    return {x, y, x + videoWidth, y + videoHeight};
}

bool VideoPositionFixer::needsPositionFix(int videoWidth, int videoHeight) {
    // Videos that need special positioning handling
    return (videoWidth == 320 && videoHeight == 180) ||  // CINEMA1 briefing videos
           (videoWidth == 76 && videoHeight == 54) ||    // Tiny pilot videos
           (videoWidth < 100 || videoHeight < 100);      // Any very small videos
}

VideoType VideoPositionFixer::detectVideoType(const std::string& filename, int width, int height) {
    // Convert filename to lowercase for comparison
    std::string lowerName = filename;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

    // Extract just the filename without path
    size_t lastSlash = lowerName.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        lowerName = lowerName.substr(lastSlash + 1);
    }

    // Remove .mp4 extension
    size_t ext = lowerName.find(".mp4");
    if (ext != std::string::npos) {
        lowerName = lowerName.substr(0, ext);
    }

    std::cout << "[VideoPositionFixer] Detecting type for: " << lowerName
              << " (" << width << "x" << height << ")" << std::endl;

    // Intro videos (fullscreen)
    if (isIntroVideo(lowerName)) {
        return VideoType::INTRO_FULLSCREEN;
    }

    // Pilot videos (tiny avatars)
    if (isPilotVideo(lowerName) || (width <= 100 && height <= 100)) {
        return VideoType::PILOT_AVATAR;
    }

    // Briefing videos (small, but not tiny)
    if (isBriefingVideo(lowerName) || (width == 320 && height == 180)) {
        return VideoType::BRIEFING_WINDOW;
    }

    // Large videos are likely cutscenes
    if (width >= 640 && height >= 480) {
        return VideoType::CUTSCENE_FULLSCREEN;
    }

    return VideoType::UNKNOWN;
}

bool VideoPositionFixer::isIntroVideo(const std::string& filename) {
    return filename == "msft" ||
           filename.find("cinema") == 0 ||  // cinema1, cinema2, etc.
           filename == "credits" ||
           filename == "planet";
}

bool VideoPositionFixer::isPilotVideo(const std::string& filename) {
    // Known pilot video name patterns
    static const std::string pilotNames[] = {
        "bubba", "choice", "chopper", "claymor", "cobra", "creep", "dagger",
        "flash", "ghost", "hacksaw", "hammer", "jinx", "longsho", "meat",
        "mother", "nuke", "palerid", "payback", "psycho", "rooster", "scooter",
        "shadow", "steel", "twitch", "venom", "wicked", "worm"
    };

    for (const auto& pilotName : pilotNames) {
        if (filename.find(pilotName) == 0) {
            return true;
        }
    }

    return false;
}

bool VideoPositionFixer::isBriefingVideo(const std::string& filename) {
    // Briefing videos typically have patterns like mc2_04a, node_02, etc.
    return filename.find("mc2_") == 0 ||
           filename.find("node_") == 0 ||
           filename.find("v1") == 0 ||
           filename.find("v3") == 0 ||
           filename.find("v6") == 0 ||
           filename.find("w") == 0;  // w01, w02, etc.
}