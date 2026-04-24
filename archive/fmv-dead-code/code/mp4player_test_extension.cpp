#include "mp4player_test_extension.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

// Global instance
MP4PlayerTestExtension* g_testExtension = nullptr;

MP4PlayerTestExtension::MP4PlayerTestExtension() {
    // Initialize metrics
    metrics = VideoTestMetrics{};
}

MP4PlayerTestExtension::~MP4PlayerTestExtension() {
    // Clean up
}

void MP4PlayerTestExtension::onVideoFrameDecoded(double pts, double decode_time_ms) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);

    decodeTimesSamples.push_back(decode_time_ms);

    // Track max decode time
    if (decode_time_ms > metrics.max_decode_time_ms) {
        metrics.max_decode_time_ms = decode_time_ms;
    }

    // Calculate frame intervals for jitter analysis
    static double lastVideoPts = -1.0;
    if (lastVideoPts >= 0.0) {
        double interval = (pts - lastVideoPts) * 1000.0; // Convert to ms
        frameIntervals.push_back(interval);
    }
    lastVideoPts = pts;
}

void MP4PlayerTestExtension::onVideoFrameRendered(double pts, double render_time_ms) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);

    renderTimesSamples.push_back(render_time_ms);
    metrics.total_frames_rendered++;

    // Track max render time
    if (render_time_ms > metrics.max_render_time_ms) {
        metrics.max_render_time_ms = render_time_ms;
    }
}

void MP4PlayerTestExtension::onAudioPacketProcessed(double pts, int queue_size) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);

    // Calculate A/V sync drift
    static double lastVideoPts = -1.0;
    static double lastAudioPts = -1.0;

    if (lastVideoPts >= 0.0 && lastAudioPts >= 0.0) {
        double drift_ms = abs((pts - lastAudioPts) - (lastVideoPts - lastAudioPts)) * 1000.0;
        avSyncDrifts.push_back(drift_ms);

        if (drift_ms > metrics.max_av_drift_ms) {
            metrics.max_av_drift_ms = drift_ms;
        }

        if (drift_ms > 40.0) { // 40ms threshold
            metrics.av_sync_violations++;
        }
    }

    lastAudioPts = pts;
}

void MP4PlayerTestExtension::onAudioCallback(int bytes_requested, int bytes_provided) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);

    audioCallbackCount++;

    // Detect underruns and overruns
    if (bytes_provided < bytes_requested) {
        audioUnderruns++;
        metrics.audio_underruns = audioUnderruns;
    }

    // Simple glitch detection (too many consecutive underruns)
    static int consecutiveUnderruns = 0;
    if (bytes_provided < bytes_requested) {
        consecutiveUnderruns++;
        if (consecutiveUnderruns > 3) {
            metrics.audio_glitches++;
            consecutiveUnderruns = 0;
        }
    } else {
        consecutiveUnderruns = 0;
    }
}

void MP4PlayerTestExtension::onOpenGLError(const std::string& operation, int error_code) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);

    metrics.gl_errors++;
    metrics.gl_error_messages.push_back(operation + ": " + std::to_string(error_code));
}

void MP4PlayerTestExtension::onTextureCreated(bool success, int width, int height) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);

    metrics.texture_creation_success = success;
    if (!success) {
        metrics.failure_reasons.push_back("Texture creation failed");
    }
}

void MP4PlayerTestExtension::setExpectedPosition(int x, int y, int width, int height) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);
    metrics.expected_position = {x, y, width, height};
}

void MP4PlayerTestExtension::setActualPosition(int x, int y, int width, int height) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);
    metrics.actual_position = {x, y, width, height};

    // Check if position is correct (within tolerance)
    const int tolerance = 10; // pixels
    bool xCorrect = abs(metrics.actual_position.x - metrics.expected_position.x) <= tolerance;
    bool yCorrect = abs(metrics.actual_position.y - metrics.expected_position.y) <= tolerance;
    bool wCorrect = abs(metrics.actual_position.width - metrics.expected_position.width) <= tolerance;
    bool hCorrect = abs(metrics.actual_position.height - metrics.expected_position.height) <= tolerance;

    metrics.position_correct = xCorrect && yCorrect && wCorrect && hCorrect;

    if (!metrics.position_correct) {
        metrics.failure_reasons.push_back("Video position incorrect");
    }
}

void MP4PlayerTestExtension::updateMemoryUsage(size_t current_mb) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);
    if (current_mb > metrics.peak_memory_mb) {
        metrics.peak_memory_mb = current_mb;
    }
}

void MP4PlayerTestExtension::updateCPUUsage(double cpu_percent) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);
    // Running average
    static int samples = 0;
    metrics.avg_cpu_percent = (metrics.avg_cpu_percent * samples + cpu_percent) / (samples + 1);
    samples++;
}

void MP4PlayerTestExtension::startPlayback(double expected_duration, double fps) {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);

    playbackStartTime = std::chrono::steady_clock::now();
    metrics.expected_playback_duration = expected_duration;
    metrics.fps = fps;
    metrics.total_frames_expected = (int)(expected_duration * fps);

    // Reset counters
    audioCallbackCount = 0;
    audioUnderruns = 0;
    audioOverruns = 0;
    frameIntervals.clear();
    avSyncDrifts.clear();
    decodeTimesSamples.clear();
    renderTimesSamples.clear();
}

void MP4PlayerTestExtension::endPlayback() {
    if (!testModeEnabled) return;

    std::lock_guard<std::mutex> lock(metricsMutex);

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(endTime - playbackStartTime);
    metrics.actual_playback_duration = duration.count();

    // Calculate timing error
    if (metrics.expected_playback_duration > 0.0) {
        metrics.timing_error_percent =
            abs(metrics.actual_playback_duration - metrics.expected_playback_duration) /
            metrics.expected_playback_duration * 100.0;
    }

    // Calculate frame statistics
    if (!frameIntervals.empty()) {
        metrics.avg_frame_interval_ms = calculateAverage(frameIntervals);
        metrics.frame_jitter_stddev_ms = calculateStandardDeviation(frameIntervals);
    }

    // Calculate A/V sync statistics
    if (!avSyncDrifts.empty()) {
        metrics.avg_av_drift_ms = calculateAverage(avSyncDrifts);
    }

    // Overall pass/fail determination
    metrics.passed = metrics.failure_reasons.empty() &&
                     metrics.timing_error_percent < 5.0 && // Within 5% of expected duration
                     metrics.max_av_drift_ms < 100.0 &&    // A/V drift under 100ms
                     metrics.av_sync_violations < 10 &&    // Less than 10 sync violations
                     metrics.texture_creation_success &&
                     metrics.gl_errors == 0;

    if (!metrics.passed && metrics.failure_reasons.empty()) {
        // Add generic failure reasons based on thresholds
        if (metrics.timing_error_percent >= 5.0) {
            metrics.failure_reasons.push_back("Timing error too high: " + std::to_string(metrics.timing_error_percent) + "%");
        }
        if (metrics.max_av_drift_ms >= 100.0) {
            metrics.failure_reasons.push_back("A/V sync drift too high: " + std::to_string(metrics.max_av_drift_ms) + "ms");
        }
        if (metrics.av_sync_violations >= 10) {
            metrics.failure_reasons.push_back("Too many A/V sync violations: " + std::to_string(metrics.av_sync_violations));
        }
    }
}

VideoTestMetrics MP4PlayerTestExtension::getMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex);
    return metrics;
}

double MP4PlayerTestExtension::calculateStandardDeviation(const std::vector<double>& values) const {
    if (values.empty()) return 0.0;

    double mean = calculateAverage(values);
    double variance = 0.0;

    for (double value : values) {
        variance += (value - mean) * (value - mean);
    }

    variance /= values.size();
    return sqrt(variance);
}

double MP4PlayerTestExtension::calculateAverage(const std::vector<double>& values) const {
    if (values.empty()) return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}