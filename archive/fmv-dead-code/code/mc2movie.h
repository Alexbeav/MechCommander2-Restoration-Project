#ifndef MC2MOVIE_H
#define MC2MOVIE_H

#include <string>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <windows.h>
#include "gamesound.h"
#include "debugging.h"
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>

class MC2Movie {
public:
    MC2Movie(const std::string& moviePath = "", SDL_Window* window = nullptr, SDL_GLContext context = nullptr);
    ~MC2Movie();

    bool init(const std::string& path, RECT& rect, bool looped, SDL_Window* window = nullptr, SDL_GLContext context = nullptr);
    std::string getMovieName() const;
    void setFrameData(uint8_t* data, int width, int height);
    void BltMovieFrame();
    void render();
    void update();
    void end();
    void stop();
    void pause();
    void restart();
    bool isPlaying() const;
    bool getIsLooped() const { return isLooped; }
    bool isInitialized() const { return initialized; }

    RECT MC2Rect;

    // Add new public methods
    void setFrameRate(double fps);

private:
    void CheckGLError(const std::string& operation);
    void cleanup();
    void initializeTexture(int width, int height);
    void saveOpenGLState();
    void restoreOpenGLState();

    std::string moviePath;
    SDL_Window* sdlWindow;        // Main game window
    SDL_GLContext glContext;      // Main game context
    GLuint textureID;
    uint8_t* frameRGBData;
    int frameWidth;
    int frameHeight;
    std::mutex textureMutex;
    std::atomic<bool> frameReady{false};
    std::atomic<bool> isPlayingFlag{false};
    std::atomic<bool> isPaused{false};
    bool isLooped;
    bool initialized;
    bool textureInitialized;
    void* soundSystem;

    // Saved OpenGL states
    struct OpenGLState {
        GLint viewport[4];
        GLint blend;
        GLint texture_2d;
        GLint depth_test;
        GLfloat clear_color[4];
        GLint matrix_mode;
        GLint active_texture;
        GLint texture_binding_2d;
    } savedState;

    // Frame timing members
    double frameTime{1.0/30.0};  // Default to 30fps
    std::chrono::steady_clock::time_point lastFrameTime;

    
    // Add frame statistics
    struct FrameStats {
        uint64_t totalFrames{0};
        uint64_t droppedFrames{0};
        double averageFrameTime{0};
    } stats;
};

#endif