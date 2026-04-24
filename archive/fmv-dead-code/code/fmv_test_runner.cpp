// FMV Test Runner - Command-line tool for automated video testing
// Usage: fmv_test_runner.exe [options]

#include "fmv_test_framework.cpp"
#include <iostream>
#include <string>
#include <vector>

// Command-line options
struct TestOptions {
    bool headlessMode = true;
    bool verbose = false;
    bool generateReport = true;
    bool quickValidationOnly = false;
    std::string outputDir = ".";
    std::vector<std::string> specificFiles;
    std::string testSuite = "all"; // all, intro, briefing, pilot, stress
};

class FMVTestRunner {
public:
    static int main(int argc, char* argv[]) {
        TestOptions options = parseCommandLine(argc, argv);

        if (options.verbose) {
            std::cout << "MechCommander 2 FMV Test Runner" << std::endl;
            std::cout << "===============================" << std::endl;
        }

        FMVTestFramework framework;
        std::vector<VideoTestMetrics> allResults;

        try {
            // Initialize headless SDL if needed
            if (options.headlessMode) {
                initHeadlessEnvironment();
            }

            // Run tests based on suite selection
            if (options.testSuite == "all" || options.testSuite == "validation") {
                if (options.verbose) std::cout << "Running file validation tests..." << std::endl;
                allResults.push_back(testKnownVideos());
            }

            if (options.testSuite == "all" || options.testSuite == "intro") {
                if (options.verbose) std::cout << "Running intro sequence tests..." << std::endl;
                allResults.push_back(framework.testIntroSequence());
            }

            if (options.testSuite == "all" || options.testSuite == "briefing") {
                if (options.verbose) std::cout << "Running briefing video tests..." << std::endl;
                // Test all briefing videos found
                auto briefingVideos = findBriefingVideos();
                for (const auto& video : briefingVideos) {
                    allResults.push_back(framework.testBriefingVideo(video));
                }
            }

            if (options.testSuite == "all" || options.testSuite == "pilot") {
                if (options.verbose) std::cout << "Running pilot video tests..." << std::endl;
                auto pilotVideos = findPilotVideos();
                for (const auto& video : pilotVideos) {
                    allResults.push_back(framework.testPilotVideo(video));
                }
            }

            if (options.testSuite == "stress") {
                if (options.verbose) std::cout << "Running stress tests..." << std::endl;
                auto stressResults = framework.testStressScenarios();
                allResults.insert(allResults.end(), stressResults.begin(), stressResults.end());
            }

            // Test specific files if provided
            for (const auto& file : options.specificFiles) {
                if (options.verbose) std::cout << "Testing specific file: " << file << std::endl;
                if (options.quickValidationOnly) {
                    allResults.push_back(VideoValidator::validateFile(file));
                } else {
                    allResults.push_back(framework.testBriefingVideo(file)); // Use generic test
                }
            }

            // Generate reports
            if (options.generateReport) {
                framework.generateTestReport(allResults);
                generateJUnitReport(allResults, options.outputDir + "/fmv_junit.xml");
                generateCIReport(allResults, options.outputDir + "/fmv_ci_summary.json");
            }

            // Print summary
            printSummary(allResults, options.verbose);

            // Return appropriate exit code
            return calculateExitCode(allResults);

        } catch (const std::exception& e) {
            std::cerr << "ERROR: " << e.what() << std::endl;
            return 1;
        }
    }

private:
    static TestOptions parseCommandLine(int argc, char* argv[]) {
        TestOptions options;

        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                printUsage();
                exit(0);
            } else if (arg == "--headless") {
                options.headlessMode = true;
            } else if (arg == "--visual") {
                options.headlessMode = false;
            } else if (arg == "--verbose" || arg == "-v") {
                options.verbose = true;
            } else if (arg == "--no-report") {
                options.generateReport = false;
            } else if (arg == "--quick") {
                options.quickValidationOnly = true;
            } else if (arg == "--suite") {
                if (i + 1 < argc) {
                    options.testSuite = argv[++i];
                }
            } else if (arg == "--output") {
                if (i + 1 < argc) {
                    options.outputDir = argv[++i];
                }
            } else if (arg == "--file") {
                if (i + 1 < argc) {
                    options.specificFiles.push_back(argv[++i]);
                }
            }
        }

        return options;
    }

    static void printUsage() {
        std::cout << "FMV Test Runner - MechCommander 2 Video Testing\n\n";
        std::cout << "Usage: fmv_test_runner [options]\n\n";
        std::cout << "Options:\n";
        std::cout << "  --help, -h          Show this help message\n";
        std::cout << "  --headless          Run in headless mode (default)\n";
        std::cout << "  --visual            Run with visual output\n";
        std::cout << "  --verbose, -v       Verbose output\n";
        std::cout << "  --no-report         Skip generating HTML report\n";
        std::cout << "  --quick             Only run file validation (fast)\n";
        std::cout << "  --suite SUITE       Test suite: all, intro, briefing, pilot, stress, validation\n";
        std::cout << "  --output DIR        Output directory for reports\n";
        std::cout << "  --file FILE         Test specific file (can be used multiple times)\n\n";
        std::cout << "Examples:\n";
        std::cout << "  fmv_test_runner --quick                    # Fast file validation only\n";
        std::cout << "  fmv_test_runner --suite intro --verbose    # Test intro sequence with details\n";
        std::cout << "  fmv_test_runner --file video.mp4          # Test specific file\n";
        std::cout << "  fmv_test_runner --visual --suite stress   # Visual stress testing\n";
    }

    static void initHeadlessEnvironment() {
        // Initialize SDL in headless mode
        if (SDL_Init(SDL_INIT_AUDIO) < 0) {
            throw std::runtime_error("Failed to initialize SDL audio: " + std::string(SDL_GetError()));
        }

        // Set SDL to use software rendering for headless testing
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

        std::cout << "Initialized headless test environment" << std::endl;
    }

    static std::vector<std::string> findBriefingVideos() {
        // Find all briefing videos in the game directory
        // This would scan for video files in briefing directories
        std::vector<std::string> videos;

        // Add known briefing video patterns
        // You would implement actual directory scanning here

        return videos;
    }

    static std::vector<std::string> findPilotVideos() {
        // Find all pilot videos in the game directory
        std::vector<std::string> videos;

        // Add known pilot video patterns
        // You would implement actual directory scanning here

        return videos;
    }

    static void generateJUnitReport(const std::vector<VideoTestMetrics>& results, const std::string& filename) {
        std::ofstream junit(filename);

        junit << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        junit << "<testsuite name=\"FMV Tests\" tests=\"" << results.size() << "\"";

        int failures = 0;
        for (const auto& result : results) {
            if (!result.passed) failures++;
        }

        junit << " failures=\"" << failures << "\">\n";

        for (const auto& result : results) {
            junit << "  <testcase classname=\"FMV\" name=\"" << result.filename << "\"";

            if (result.passed) {
                junit << "/>\n";
            } else {
                junit << ">\n";
                junit << "    <failure message=\"Test failed\">\n";
                for (const auto& reason : result.failure_reasons) {
                    junit << "      " << reason << "\n";
                }
                junit << "    </failure>\n";
                junit << "  </testcase>\n";
            }
        }

        junit << "</testsuite>\n";
        junit.close();

        std::cout << "JUnit report generated: " << filename << std::endl;
    }

    static void generateCIReport(const std::vector<VideoTestMetrics>& results, const std::string& filename) {
        std::ofstream ci(filename);

        int passed = 0, failed = 0;
        for (const auto& result : results) {
            if (result.passed) passed++; else failed++;
        }

        ci << "{\n";
        ci << "  \"total_tests\": " << results.size() << ",\n";
        ci << "  \"passed\": " << passed << ",\n";
        ci << "  \"failed\": " << failed << ",\n";
        ci << "  \"success_rate\": " << (100.0 * passed / results.size()) << ",\n";
        ci << "  \"timestamp\": \"" << getCurrentTimestamp() << "\",\n";
        ci << "  \"results\": [\n";

        for (size_t i = 0; i < results.size(); i++) {
            ci << "    " << results[i].toJson();
            if (i < results.size() - 1) ci << ",";
            ci << "\n";
        }

        ci << "  ]\n";
        ci << "}\n";
        ci.close();

        std::cout << "CI report generated: " << filename << std::endl;
    }

    static void printSummary(const std::vector<VideoTestMetrics>& results, bool verbose) {
        int passed = 0, failed = 0;
        for (const auto& result : results) {
            if (result.passed) passed++; else failed++;
        }

        std::cout << "\n=== FMV Test Summary ===" << std::endl;
        std::cout << "Total Tests: " << results.size() << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Success Rate: " << (100.0 * passed / results.size()) << "%" << std::endl;

        if (failed > 0) {
            std::cout << "\nFailures:" << std::endl;
            for (const auto& result : results) {
                if (!result.passed) {
                    std::cout << "  " << result.filename << ":" << std::endl;
                    for (const auto& reason : result.failure_reasons) {
                        std::cout << "    - " << reason << std::endl;
                    }
                }
            }
        }

        if (verbose && passed > 0) {
            std::cout << "\nPassed Tests:" << std::endl;
            for (const auto& result : results) {
                if (result.passed) {
                    std::cout << "  " << result.filename;
                    if (result.fps > 0) {
                        std::cout << " (" << result.fps << " FPS, " << result.duration_seconds << "s)";
                    }
                    std::cout << std::endl;
                }
            }
        }
    }

    static int calculateExitCode(const std::vector<VideoTestMetrics>& results) {
        // Return 0 if all tests passed, 1 if any failed
        for (const auto& result : results) {
            if (!result.passed) return 1;
        }
        return 0;
    }

    static std::string getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::string timeStr = std::ctime(&time_t);
        // Remove newline
        if (!timeStr.empty() && timeStr.back() == '\n') {
            timeStr.pop_back();
        }
        return timeStr;
    }
};

// Main entry point
int main(int argc, char* argv[]) {
    return FMVTestRunner::main(argc, argv);
}