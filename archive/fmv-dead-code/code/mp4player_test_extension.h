#ifndef MP4PLAYER_TEST_EXTENSION_H
#define MP4PLAYER_TEST_EXTENSION_H

#include "test_metrics.h"
#include <chrono>
#include <vector>
#include <mutex>

// Extension to MP4Player for collecting test metrics during playback
class MP4PlayerTestExtension {
public:
    MP4PlayerTestExtension();
    ~MP4PlayerTestExtension();

    // Call these from MP4Player methods to collect metrics
    void onVideoFrameDecoded(double pts, double decode_time_ms);
    void onVideoFrameRendered(double pts, double render_time_ms);
    void onAudioPacketProcessed(double pts, int queue_size);
    void onAudioCallback(int bytes_requested, int bytes_provided);
    void onOpenGLError(const std::string& operation, int error_code);
    void onTextureCreated(bool success, int width, int height);

    // Position tracking for briefing videos
    void setExpectedPosition(int x, int y, int width, int height);
    void setActualPosition(int x, int y, int width, int height);

    // Memory and performance tracking
    void updateMemoryUsage(size_t current_mb);
    void updateCPUUsage(double cpu_percent);

    // Start/stop timing
    void startPlayback(double expected_duration, double fps);
    void endPlayback();

    // Get final metrics
    VideoTestMetrics getMetrics() const;

    // Enable/disable testing mode
    void setTestMode(bool enabled) { testModeEnabled = enabled; }
    bool isTestMode() const { return testModeEnabled; }

private:
    mutable std::mutex metricsMutex;
    VideoTestMetrics metrics;
    bool testModeEnabled = false;

    // Timing tracking
    std::chrono::steady_clock::time_point playbackStartTime;
    std::vector<double> frameIntervals;
    std::vector<double> avSyncDrifts;

    // Performance tracking
    std::vector<double> decodeTimesSamples;
    std::vector<double> renderTimesSamples;

    // Audio tracking
    int audioCallbackCount = 0;
    int audioUnderruns = 0;
    int audioOverruns = 0;

    // Helper methods
    double calculateStandardDeviation(const std::vector<double>& values) const;
    double calculateAverage(const std::vector<double>& values) const;
};

// Global test extension instance (can be enabled/disabled)
extern MP4PlayerTestExtension* g_testExtension;

// Convenience macros for adding to existing MP4Player code
#define TEST_METRIC_VIDEO_DECODED(pts, decode_ms) \
    if (g_testExtension && g_testExtension->isTestMode()) \
        g_testExtension->onVideoFrameDecoded(pts, decode_ms)

#define TEST_METRIC_VIDEO_RENDERED(pts, render_ms) \
    if (g_testExtension && g_testExtension->isTestMode()) \
        g_testExtension->onVideoFrameRendered(pts, render_ms)

#define TEST_METRIC_AUDIO_PROCESSED(pts, queue_size) \
    if (g_testExtension && g_testExtension->isTestMode()) \
        g_testExtension->onAudioPacketProcessed(pts, queue_size)

#define TEST_METRIC_AUDIO_CALLBACK(requested, provided) \
    if (g_testExtension && g_testExtension->isTestMode()) \
        g_testExtension->onAudioCallback(requested, provided)

#define TEST_METRIC_GL_ERROR(operation, error) \
    if (g_testExtension && g_testExtension->isTestMode()) \
        g_testExtension->onOpenGLError(operation, error)

#define TEST_METRIC_TEXTURE_CREATED(success, w, h) \
    if (g_testExtension && g_testExtension->isTestMode()) \
        g_testExtension->onTextureCreated(success, w, h)

#endif