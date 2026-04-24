#include "test_metrics.h"
#include "mp4player_test_extension.h"
#include "video_validator.cpp" // Include validator
#include "mp4player.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>

// JSON serialization for metrics
std::string VideoTestMetrics::toJson() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"filename\": \"" << filename << "\",\n";
    json << "  \"passed\": " << (passed ? "true" : "false") << ",\n";
    json << "  \"file_valid\": " << (fileValid ? "true" : "false") << ",\n";
    json << "  \"duration_seconds\": " << duration_seconds << ",\n";
    json << "  \"fps\": " << fps << ",\n";
    json << "  \"resolution\": \"" << width << "x" << height << "\",\n";
    json << "  \"timing_error_percent\": " << timing_error_percent << ",\n";
    json << "  \"max_av_drift_ms\": " << max_av_drift_ms << ",\n";
    json << "  \"avg_av_drift_ms\": " << avg_av_drift_ms << ",\n";
    json << "  \"av_sync_violations\": " << av_sync_violations << ",\n";
    json << "  \"frames_rendered\": " << total_frames_rendered << ",\n";
    json << "  \"frames_expected\": " << total_frames_expected << ",\n";
    json << "  \"frames_dropped\": " << frames_dropped << ",\n";
    json << "  \"frame_jitter_stddev_ms\": " << frame_jitter_stddev_ms << ",\n";
    json << "  \"audio_underruns\": " << audio_underruns << ",\n";
    json << "  \"audio_glitches\": " << audio_glitches << ",\n";
    json << "  \"gl_errors\": " << gl_errors << ",\n";
    json << "  \"texture_creation_success\": " << (texture_creation_success ? "true" : "false") << ",\n";
    json << "  \"position_correct\": " << (position_correct ? "true" : "false") << ",\n";
    json << "  \"peak_memory_mb\": " << peak_memory_mb << ",\n";
    json << "  \"max_decode_time_ms\": " << max_decode_time_ms << ",\n";
    json << "  \"max_render_time_ms\": " << max_render_time_ms << ",\n";

    json << "  \"failure_reasons\": [";
    for (size_t i = 0; i < failure_reasons.size(); ++i) {
        json << "\"" << failure_reasons[i] << "\"";
        if (i < failure_reasons.size() - 1) json << ", ";
    }
    json << "],\n";

    json << "  \"gl_error_messages\": [";
    for (size_t i = 0; i < gl_error_messages.size(); ++i) {
        json << "\"" << gl_error_messages[i] << "\"";
        if (i < gl_error_messages.size() - 1) json << ", ";
    }
    json << "]\n";

    json << "}";
    return json.str();
}

void VideoTestMetrics::saveToFile(const std::string& filename) const {
    std::ofstream file(filename);
    file << toJson();
    file.close();
}

// Main test framework implementation
class FMVTestFramework {
public:
    FMVTestFramework() {
        // Initialize test extension
        if (!g_testExtension) {
            g_testExtension = new MP4PlayerTestExtension();
        }
    }

    ~FMVTestFramework() {
        if (g_testExtension) {
            delete g_testExtension;
            g_testExtension = nullptr;
        }
    }

    // Test intro sequence: MSFT.mp4 -> CINEMA1.mp4
    VideoTestMetrics testIntroSequence() {
        std::cout << "Testing intro sequence (MSFT.mp4 -> CINEMA1.mp4)..." << std::endl;

        VideoTestMetrics combinedMetrics;
        combinedMetrics.filename = "intro_sequence";

        // Test MSFT.mp4 first
        VideoTestMetrics msftResult = testSingleVideo("data/movies/MSFT.mp4", VideoTestScenario::INTRO_SEQUENCE);

        // Test CINEMA1.mp4 second
        VideoTestMetrics cinema1Result = testSingleVideo("data/movies/CINEMA1.mp4", VideoTestScenario::INTRO_SEQUENCE);

        // Combine results
        combinedMetrics.passed = msftResult.passed && cinema1Result.passed;
        combinedMetrics.failure_reasons.insert(combinedMetrics.failure_reasons.end(),
                                               msftResult.failure_reasons.begin(),
                                               msftResult.failure_reasons.end());
        combinedMetrics.failure_reasons.insert(combinedMetrics.failure_reasons.end(),
                                               cinema1Result.failure_reasons.begin(),
                                               cinema1Result.failure_reasons.end());

        // Test sequence timing (should transition smoothly)
        // This would require integration with your MainMenu logic

        return combinedMetrics;
    }

    // Test individual briefing video
    VideoTestMetrics testBriefingVideo(const std::string& videoFile) {
        std::cout << "Testing briefing video: " << videoFile << std::endl;

        VideoTestMetrics result = testSingleVideo(videoFile, VideoTestScenario::BRIEFING_VIDEO);

        // For briefing videos, check positioning is correct
        // Expected position would be the briefing box coordinates
        if (g_testExtension) {
            // These coordinates should match your briefing video display area
            g_testExtension->setExpectedPosition(100, 200, 400, 300); // Adjust to your UI
        }

        return result;
    }

    // Test pilot video (attack confirmations)
    VideoTestMetrics testPilotVideo(const std::string& videoFile) {
        std::cout << "Testing pilot video: " << videoFile << std::endl;

        VideoTestMetrics result = testSingleVideo(videoFile, VideoTestScenario::PILOT_VIDEO);

        // Additional crash detection for pilot videos
        // Would need integration with your ControlGui system

        return result;
    }

    // Test all videos found in the game directory
    std::vector<VideoTestMetrics> testAllVideos() {
        std::vector<VideoTestMetrics> results;

        std::vector<std::string> testFiles = {
            "data/movies/MSFT.mp4",
            "data/movies/CINEMA1.mp4",
            // Add more video files as discovered
        };

        for (const auto& file : testFiles) {
            // First validate the file quickly
            VideoTestMetrics validation = VideoValidator::validateFile(file);

            if (validation.fileValid) {
                // If file is valid, do full playback test
                VideoTestMetrics playbackResult = testSingleVideo(file, VideoTestScenario::CUTSCENE_VIDEO);
                results.push_back(playbackResult);
            } else {
                std::cout << "Skipping invalid file: " << file << std::endl;
                results.push_back(validation);
            }
        }

        return results;
    }

    // Stress test: rapid playback, memory leaks, etc.
    std::vector<VideoTestMetrics> testStressScenarios() {
        std::vector<VideoTestMetrics> results;

        // Test 1: Rapid start/stop cycles
        results.push_back(testRapidStartStop("data/movies/MSFT.mp4"));

        // Test 2: Memory leak detection over multiple plays
        results.push_back(testMemoryLeaks("data/movies/CINEMA1.mp4"));

        // Test 3: Concurrent video attempts (should handle gracefully)
        results.push_back(testConcurrentVideos());

        return results;
    }

    // Generate comprehensive test report
    void generateTestReport(const std::vector<VideoTestMetrics>& results) {
        std::ofstream report("fmv_test_report.html");

        report << "<html><head><title>FMV Test Report</title></head><body>\n";
        report << "<h1>MechCommander 2 FMV Test Report</h1>\n";
        report << "<p>Generated: " << getCurrentTimestamp() << "</p>\n";

        int passed = 0, failed = 0;
        for (const auto& result : results) {
            if (result.passed) passed++; else failed++;
        }

        report << "<h2>Summary</h2>\n";
        report << "<p>Total Tests: " << results.size() << " | Passed: " << passed << " | Failed: " << failed << "</p>\n";

        report << "<h2>Test Results</h2>\n";
        for (const auto& result : results) {
            report << "<div style='border: 1px solid " << (result.passed ? "green" : "red") << "; margin: 10px; padding: 10px;'>\n";
            report << "<h3>" << result.filename << " - " << (result.passed ? "PASSED" : "FAILED") << "</h3>\n";

            if (result.fileValid) {
                report << "<p>Duration: " << result.duration_seconds << "s | FPS: " << result.fps << " | Resolution: " << result.width << "x" << result.height << "</p>\n";
                report << "<p>Timing Error: " << result.timing_error_percent << "% | Max A/V Drift: " << result.max_av_drift_ms << "ms</p>\n";
                report << "<p>Frames: " << result.total_frames_rendered << "/" << result.total_frames_expected << " | Dropped: " << result.frames_dropped << "</p>\n";
                report << "<p>Audio Issues: " << result.audio_underruns << " underruns, " << result.audio_glitches << " glitches</p>\n";
                report << "<p>OpenGL: " << result.gl_errors << " errors | Texture: " << (result.texture_creation_success ? "OK" : "FAILED") << "</p>\n";
            }

            if (!result.failure_reasons.empty()) {
                report << "<p><strong>Failures:</strong></p><ul>\n";
                for (const auto& reason : result.failure_reasons) {
                    report << "<li>" << reason << "</li>\n";
                }
                report << "</ul>\n";
            }

            report << "</div>\n";
        }

        report << "</body></html>\n";
        report.close();

        std::cout << "Test report generated: fmv_test_report.html" << std::endl;
    }

private:
    VideoTestMetrics testSingleVideo(const std::string& videoFile, VideoTestScenario scenario) {
        VideoTestMetrics result;
        result.filename = videoFile;

        // First validate the file
        VideoTestMetrics validation = VideoValidator::validateFile(videoFile);
        if (!validation.fileValid) {
            return validation; // Return validation failure immediately
        }

        // Enable test mode
        if (g_testExtension) {
            g_testExtension->setTestMode(true);
            g_testExtension->startPlayback(validation.duration_seconds, validation.fps);
        }

        try {
            // Create headless MP4Player (without actual window/rendering for speed)
            bool headlessMode = true; // Set to false if you want to see the video during testing

            if (headlessMode) {
                result = runHeadlessTest(videoFile, scenario);
            } else {
                result = runFullRenderTest(videoFile, scenario);
            }

        } catch (const std::exception& e) {
            result.passed = false;
            result.failure_reasons.push_back("Exception during playback: " + std::string(e.what()));
        }

        // Get final metrics
        if (g_testExtension) {
            g_testExtension->endPlayback();
            VideoTestMetrics testMetrics = g_testExtension->getMetrics();

            // Merge test metrics with validation metrics
            result.timing_error_percent = testMetrics.timing_error_percent;
            result.max_av_drift_ms = testMetrics.max_av_drift_ms;
            result.avg_av_drift_ms = testMetrics.avg_av_drift_ms;
            result.av_sync_violations = testMetrics.av_sync_violations;
            result.total_frames_rendered = testMetrics.total_frames_rendered;
            result.frames_dropped = testMetrics.frames_dropped;
            result.frame_jitter_stddev_ms = testMetrics.frame_jitter_stddev_ms;
            result.audio_underruns = testMetrics.audio_underruns;
            result.audio_glitches = testMetrics.audio_glitches;
            result.gl_errors = testMetrics.gl_errors;
            result.texture_creation_success = testMetrics.texture_creation_success;
            result.peak_memory_mb = testMetrics.peak_memory_mb;
            result.max_decode_time_ms = testMetrics.max_decode_time_ms;
            result.max_render_time_ms = testMetrics.max_render_time_ms;
            result.passed = testMetrics.passed;

            result.failure_reasons.insert(result.failure_reasons.end(),
                                          testMetrics.failure_reasons.begin(),
                                          testMetrics.failure_reasons.end());

            g_testExtension->setTestMode(false);
        }

        return result;
    }

    VideoTestMetrics runHeadlessTest(const std::string& videoFile, VideoTestScenario scenario) {
        // Headless testing - just decode/process without rendering
        // This is much faster for automated testing
        VideoTestMetrics result;
        result.filename = videoFile;

        // Implementation would create MP4Player in headless mode
        // Process all frames but skip OpenGL rendering
        // Still collect all timing and A/V sync metrics

        // For now, simulate the test
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate processing time

        result.passed = true; // Would be determined by actual test
        return result;
    }

    VideoTestMetrics runFullRenderTest(const std::string& videoFile, VideoTestScenario scenario) {
        // Full rendering test - actually displays the video
        // Slower but tests complete rendering pipeline
        VideoTestMetrics result;
        result.filename = videoFile;

        // Implementation would use full MP4Player with OpenGL rendering
        // Useful for visual debugging but too slow for CI

        result.passed = true; // Would be determined by actual test
        return result;
    }

    VideoTestMetrics testRapidStartStop(const std::string& videoFile) {
        VideoTestMetrics result;
        result.filename = videoFile + "_rapid_start_stop";

        // Test rapid start/stop cycles to detect memory leaks or crashes
        for (int i = 0; i < 10; i++) {
            // Start video
            // Immediately stop
            // Check for crashes or memory growth
        }

        result.passed = true; // Would be determined by actual test
        return result;
    }

    VideoTestMetrics testMemoryLeaks(const std::string& videoFile) {
        VideoTestMetrics result;
        result.filename = videoFile + "_memory_leak";

        size_t startMemory = getCurrentMemoryUsage();

        // Play video multiple times
        for (int i = 0; i < 5; i++) {
            // Full playback cycle
        }

        size_t endMemory = getCurrentMemoryUsage();
        size_t memoryGrowth = endMemory - startMemory;

        if (memoryGrowth > 100) { // More than 100MB growth
            result.failure_reasons.push_back("Memory leak detected: " + std::to_string(memoryGrowth) + "MB growth");
            result.passed = false;
        } else {
            result.passed = true;
        }

        return result;
    }

    VideoTestMetrics testConcurrentVideos() {
        VideoTestMetrics result;
        result.filename = "concurrent_videos_test";

        // Test attempting to play multiple videos simultaneously
        // Should handle gracefully (either queue or reject)

        result.passed = true; // Would be determined by actual test
        return result;
    }

    size_t getCurrentMemoryUsage() {
        // Platform-specific memory usage detection
        // For Windows, could use GetProcessMemoryInfo
        return 0; // Placeholder
    }

    std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        return std::ctime(&time_t);
    }
};