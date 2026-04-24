#include "frame_rate_fix.h"
#include <iostream>
#include <algorithm>
#include <cmath>

double FrameRateFixer::getCorrectFrameTime(double detectedFps) {
    // Handle common video frame rates properly
    if (std::abs(detectedFps - 24.0) < 0.1) {
        return 1.0 / 24.0;  // Cinema standard
    } else if (std::abs(detectedFps - 15.0) < 0.1) {
        return 1.0 / 15.0;  // Game video standard
    } else if (std::abs(detectedFps - 30.0) < 0.1) {
        return 1.0 / 30.0;  // Standard video
    } else if (std::abs(detectedFps - 60.0) < 0.1) {
        return 1.0 / 60.0;  // High frame rate
    } else if (detectedFps > 0) {
        return 1.0 / detectedFps;  // Use detected rate
    }

    // Fallback to 30fps
    return 1.0 / 30.0;
}

double FrameRateFixer::getDynamicFrameTime(double videoFps, const std::string& filename) {
    double baseFps = videoFps;

    // Apply adjustments based on video type
    double adjustedFps = adjustForVideoType(baseFps, filename);

    std::cout << "[FrameRateFixer] Video: " << filename
              << " Base FPS: " << baseFps
              << " Adjusted FPS: " << adjustedFps << std::endl;

    return getCorrectFrameTime(adjustedFps);
}

bool FrameRateFixer::needsTimingFix(double fps) {
    // Videos that need special timing handling
    return std::abs(fps - 15.0) < 0.1 ||  // Game videos at 15fps
           std::abs(fps - 24.0) < 0.1 ||  // Cinema videos at 24fps
           fps < 10.0 || fps > 120.0;     // Unusual frame rates
}

FrameRateFixer::FrameRateCategory FrameRateFixer::categorizeFrameRate(double fps) {
    if (std::abs(fps - 24.0) < 0.1) {
        return FrameRateCategory::CINEMA_24FPS;
    } else if (std::abs(fps - 15.0) < 0.1) {
        return FrameRateCategory::GAME_15FPS;
    } else if (std::abs(fps - 30.0) < 0.1) {
        return FrameRateCategory::STANDARD_30FPS;
    }
    return FrameRateCategory::OTHER;
}

double FrameRateFixer::adjustForVideoType(double baseFps, const std::string& filename) {
    // Extract filename without path/extension for analysis
    std::string name = filename;
    size_t lastSlash = name.find_last_of("/\\");
    if (lastSlash != std::string::npos) {
        name = name.substr(lastSlash + 1);
    }
    size_t lastDot = name.find_last_of('.');
    if (lastDot != std::string::npos) {
        name = name.substr(0, lastDot);
    }

    // Convert to lowercase
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    // Intro videos (typically 24fps or high quality)
    if (name == "msft" || name.find("cinema") != std::string::npos) {
        // These should maintain their native frame rate
        return baseFps;
    }

    // Briefing videos (typically 15fps)
    if (name.find("mc2_") != std::string::npos ||
        name.find("node_") != std::string::npos ||
        name.find("v1") != std::string::npos ||
        name.find("v3") != std::string::npos ||
        name.find("v6") != std::string::npos) {
        // Briefing videos - ensure they're not playing too fast
        if (baseFps > 20.0) {
            std::cout << "[FrameRateFixer] Briefing video detected, capping frame rate" << std::endl;
            return 15.0;  // Cap briefing videos at 15fps
        }
        return baseFps;
    }

    // Pilot videos (typically 15fps, very short)
    if (name.find("bubba") != std::string::npos ||
        name.find("chopper") != std::string::npos ||
        name.find("cobra") != std::string::npos ||
        name.find("ghost") != std::string::npos) {
        // Pilot videos should be consistent
        return std::max(15.0, baseFps);
    }

    return baseFps;
}

// PrecisionTimer implementation
FrameRateFixer::PrecisionTimer::PrecisionTimer(double targetFps)
    : targetFrameTime(1.0 / targetFps)
    , lastFrameTime(std::chrono::high_resolution_clock::now())
    , startTime(lastFrameTime)
    , frameCount(0)
    , accumelatedError(0.0) {
}

bool FrameRateFixer::PrecisionTimer::shouldRenderFrame() {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double>(now - lastFrameTime).count();

    // Add accumulated timing error for smooth playback
    elapsed += accumelatedError;

    if (elapsed >= targetFrameTime) {
        // Calculate error for next frame
        accumelatedError = elapsed - targetFrameTime;

        // Prevent accumulating too much error
        if (accumelatedError > targetFrameTime) {
            accumelatedError = targetFrameTime;
        }

        return true;
    }

    return false;
}

void FrameRateFixer::PrecisionTimer::markFrameRendered() {
    lastFrameTime = std::chrono::high_resolution_clock::now();
    frameCount++;
}

double FrameRateFixer::PrecisionTimer::getActualFps() const {
    auto now = std::chrono::high_resolution_clock::now();
    auto totalTime = std::chrono::duration<double>(now - startTime).count();

    if (totalTime > 0.0 && frameCount > 0) {
        return frameCount / totalTime;
    }

    return 0.0;
}

void FrameRateFixer::PrecisionTimer::reset() {
    lastFrameTime = std::chrono::high_resolution_clock::now();
    startTime = lastFrameTime;
    frameCount = 0;
    accumelatedError = 0.0;
}