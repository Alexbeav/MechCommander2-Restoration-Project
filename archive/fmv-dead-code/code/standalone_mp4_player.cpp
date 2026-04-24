#include "mp4player.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <windows.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static void printUsage() {
    std::cout << "Usage: mp4_standalone <video_path> [width] [height] [--duration N] [--expect_frames N] [--log file]\n";
}

static std::string g_logPath;

static LONG WINAPI StandaloneExceptionHandler(EXCEPTION_POINTERS* ex) {
    DWORD code = ex ? ex->ExceptionRecord->ExceptionCode : 0;
    std::cerr << "[Standalone] Crash (SEH) code: 0x" << std::hex << code << std::dec << "\n";
    if (!g_logPath.empty()) {
        FILE* f = fopen(g_logPath.c_str(), "a");
        if (f) {
            fprintf(f, "crash=0x%08lx\n", code);
            fclose(f);
        }
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string videoPath;
    int windowWidth = 1280;
    int windowHeight = 720;
    double maxRunSeconds = 15.0;
    int expectFrames = 0;
    std::string logPath;

    std::vector<std::string> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];
        if (arg == "--duration" && i + 1 < args.size()) {
            maxRunSeconds = std::atof(args[++i].c_str());
        } else if (arg == "--expect_frames" && i + 1 < args.size()) {
            expectFrames = std::atoi(args[++i].c_str());
        } else if (arg == "--log" && i + 1 < args.size()) {
            logPath = args[++i];
        } else if (arg == "--width" && i + 1 < args.size()) {
            windowWidth = std::atoi(args[++i].c_str());
        } else if (arg == "--height" && i + 1 < args.size()) {
            windowHeight = std::atoi(args[++i].c_str());
        } else if (!arg.empty() && arg[0] != '-') {
            if (videoPath.empty()) {
                videoPath = arg;
            } else if (windowWidth == 1280 && windowHeight == 720) {
                windowWidth = std::atoi(arg.c_str());
                if (i + 1 < args.size()) {
                    windowHeight = std::atoi(args[++i].c_str());
                }
            }
        }
    }

    if (videoPath.empty()) {
        printUsage();
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "MC2 MP4 Standalone",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        windowWidth,
        windowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(1);

    // Initialize GLEW for function pointers like glActiveTexture
    GLenum glewErr = glewInit();
    if (glewErr != GLEW_OK) {
        std::cerr << "glewInit failed: " << glewGetErrorString(glewErr) << "\n";
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    RECT movieRect = {0, 0, windowWidth, windowHeight};
    MP4Player player(videoPath, window, context);
    player.init(videoPath.c_str(), movieRect, false, window, context);
    std::cout << "[Standalone] isPlaying after init: " << (player.isPlaying() ? "true" : "false") << "\n";

    std::ofstream logFile;
    if (!logPath.empty()) {
        logFile.open(logPath, std::ios::out | std::ios::trunc);
        if (logFile.is_open()) {
            logFile << "video=" << videoPath << "\n";
            logFile << "width=" << windowWidth << "\n";
            logFile << "height=" << windowHeight << "\n";
            logFile << "duration=" << maxRunSeconds << "\n";
        }
    }
    g_logPath = logPath;
    SetUnhandledExceptionFilter(StandaloneExceptionHandler);

    bool running = true;
    auto startTime = std::chrono::steady_clock::now();
    bool sawPlaybackStop = false;
    int renderFrames = 0;

    while (running) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - startTime).count();
        if (elapsed > maxRunSeconds) {
            std::cout << "[Standalone] Timeout reached at " << elapsed << "s\n";
            break;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_SPACE) {
                    running = false;
                    break;
                }
            }
        }

        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (player.isPlaying()) {
            player.update();
            player.render();
            renderFrames++;
        } else {
            if (!sawPlaybackStop) {
                sawPlaybackStop = true;
                double playbackTime = std::chrono::duration<double>(now - startTime).count();
                int decodedFrames = player.getFrameCount();
                double actualFps = decodedFrames / playbackTime;
                std::cout << "[Standalone] Playback finished: " << decodedFrames << " frames in "
                          << playbackTime << "s (" << actualFps << " FPS, target: "
                          << player.frameRate << " FPS)\n";
            }
            if (elapsed > 2.0) {
                break;
            }
        }

        SDL_GL_SwapWindow(window);
        // No SDL_Delay - vsync (SetSwapInterval) handles frame pacing,
        // and PTS timing in MP4Player controls video frame display rate
    }

    int decodedFrames = player.getFrameCount();
    std::cout << "[Standalone] Decoded frames: " << decodedFrames << "\n";
    if (logFile.is_open()) {
        logFile << "decoded_frames=" << decodedFrames << "\n";
        logFile << "playing_at_end=" << (player.isPlaying() ? "true" : "false") << "\n";
        logFile.close();
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    if (expectFrames > 0 && decodedFrames < expectFrames) {
        return 1;
    }
    return 0;
}
