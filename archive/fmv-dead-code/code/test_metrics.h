#ifndef TEST_METRICS_H
#define TEST_METRICS_H

#include <chrono>
#include <string>
#include <vector>
#include <map>

// Test metrics collection for automated FMV testing
struct VideoTestMetrics {
    // File validation
    std::string filename;
    bool fileValid = false;
    double duration_seconds = 0.0;
    int width = 0, height = 0;
    double fps = 0.0;

    // Timing metrics
    double actual_playback_duration = 0.0;
    double expected_playback_duration = 0.0;
    double timing_error_percent = 0.0;

    // A/V sync metrics
    double max_av_drift_ms = 0.0;
    double avg_av_drift_ms = 0.0;
    int av_sync_violations = 0; // drift > 40ms

    // Frame metrics
    int total_frames_expected = 0;
    int total_frames_rendered = 0;
    int frames_dropped = 0;
    int frames_duplicated = 0;
    double avg_frame_interval_ms = 0.0;
    double frame_jitter_stddev_ms = 0.0;

    // Audio metrics
    int audio_underruns = 0;
    int audio_overruns = 0;
    int audio_glitches = 0;
    double audio_latency_ms = 0.0;

    // OpenGL metrics
    bool texture_creation_success = false;
    int gl_errors = 0;
    std::vector<std::string> gl_error_messages;

    // Positioning metrics (for briefing videos)
    struct Position { int x, y, width, height; } expected_position, actual_position;
    bool position_correct = false;

    // Memory/performance metrics
    size_t peak_memory_mb = 0;
    double avg_cpu_percent = 0.0;
    double max_decode_time_ms = 0.0;
    double max_render_time_ms = 0.0;

    // Test result
    bool passed = false;
    std::vector<std::string> failure_reasons;

    // Export to JSON for CI/automated analysis
    std::string toJson() const;
    void saveToFile(const std::string& filename) const;
};

// Test scenarios for different FMV contexts
enum class VideoTestScenario {
    INTRO_SEQUENCE,     // MSFT.mp4 -> CINEMA1.mp4 -> Menu
    BRIEFING_VIDEO,     // In-game mission briefings
    PILOT_VIDEO,        // Attack order confirmations
    CUTSCENE_VIDEO,     // General in-game cinematics
    STRESS_TEST         // Multiple rapid plays
};

// Forward declaration
class FMVTestFramework;

#endif