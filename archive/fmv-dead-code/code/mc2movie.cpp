#include "mc2movie.h"
#include <iostream>
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <mutex>
#include <chrono>

MC2Movie::MC2Movie(const std::string& moviePath, SDL_Window* window, SDL_GLContext context)
    : moviePath(moviePath), sdlWindow(window), glContext(context), textureID(0),
      frameRGBData(nullptr), frameWidth(0), frameHeight(0), frameReady(false),
      isPlayingFlag(false), isPaused(false), isLooped(false), initialized(false),
      textureInitialized(false), soundSystem(nullptr) {
    
    if (window && context) {
        SDL_GL_MakeCurrent(window, context);
        initializeTexture(1, 1);  // Initial 1x1 texture, will be resized as needed
        initialized = true;
    }
}

MC2Movie::~MC2Movie() {
    cleanup();
}

void MC2Movie::cleanup() {
    if (frameRGBData) {
        delete[] frameRGBData;
        frameRGBData = nullptr;
    }
    
    if (textureID && textureInitialized) {
        glDeleteTextures(1, &textureID);
        textureID = 0;
        textureInitialized = false;
    }
    
    frameWidth = 0;
    frameHeight = 0;
    frameReady = false;
    initialized = false;
}

void MC2Movie::initializeTexture(int width, int height) {
    // Ensure we have a valid OpenGL context
    if (!sdlWindow || !glContext) {
        std::cerr << "[MC2Movie] ERROR: No valid OpenGL context for texture creation\n";
        return;
    }
    
    // Make sure the context is current
    if (SDL_GL_MakeCurrent(sdlWindow, glContext) != 0) {
        std::cerr << "[MC2Movie] ERROR: Failed to make OpenGL context current: " << SDL_GetError() << "\n";
        return;
    }
    
    // Verify we have a working OpenGL context
    const char* glVersion = (const char*)glGetString(GL_VERSION);
    if (!glVersion) {
        std::cerr << "[MC2Movie] ERROR: No OpenGL version string - invalid context\n";
        return;
    }
    std::cout << "[MC2Movie] OpenGL Version: " << glVersion << "\n";
    
    // Clean up existing texture
    if (textureInitialized && textureID) {
        glDeleteTextures(1, &textureID);
        textureInitialized = false;
        textureID = 0;
    }

    // Clear any existing OpenGL errors
    while (glGetError() != GL_NO_ERROR) {}

    // Generate new texture
    glGenTextures(1, &textureID);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR || textureID == 0) {
        std::cerr << "[MC2Movie] STOP" << "Failed to create texture - glGenTextures error: 0x" 
                  << std::hex << error << std::dec << ", textureID: " << textureID << "\n";
        return;
    }
    
    std::cout << "[MC2Movie] Generated texture ID: " << textureID << "\n";
    
    // Bind and configure texture
    glBindTexture(GL_TEXTURE_2D, textureID);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[MC2Movie] ERROR: Failed to bind texture: 0x" << std::hex << error << std::dec << "\n";
        glDeleteTextures(1, &textureID);
        textureID = 0;
        return;
    }
    
    // Set pixel storage parameters
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    
    // Allocate texture storage - use safe dimensions
    int safeWidth = std::max(1, std::min(width, 2048));
    int safeHeight = std::max(1, std::min(height, 2048));
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, safeWidth, safeHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[MC2Movie] ERROR: Failed to allocate texture storage (" << safeWidth << "x" << safeHeight 
                  << "): 0x" << std::hex << error << std::dec << "\n";
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &textureID);
        textureID = 0;
        return;
    }
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Verify texture was created correctly
    GLint texWidth, texHeight;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[MC2Movie] ERROR: OpenGL error after texture setup: 0x" << std::hex << error << std::dec << "\n";
        glDeleteTextures(1, &textureID);
        textureID = 0;
        return;
    }
    
    textureInitialized = true;
    std::cout << "[MC2Movie] Texture successfully created - ID: " << textureID 
              << ", Size: " << texWidth << "x" << texHeight << "\n";
}

bool MC2Movie::init(const std::string& path, RECT& rect, bool looped, SDL_Window* window, SDL_GLContext context) {
    std::cout << "[MC2Movie] Initializing with path: " << path << "\n";
    moviePath = path;
    MC2Rect = rect;
    isLooped = looped;

    if (window && context) {
        sdlWindow = window;
        glContext = context;
        if (SDL_GL_MakeCurrent(window, context) != 0) {
            std::cerr << "[MC2Movie] Failed to make context current: " << SDL_GetError() << "\n";
            return false;
        }
        std::cout << "[MC2Movie] OpenGL context made current\n";
        
        initializeTexture(1, 1);  // Initial 1x1 texture, will be resized as needed
        initialized = true;
        std::cout << "[MC2Movie] Texture initialized\n";
    } else {
        std::cerr << "[MC2Movie] Invalid window (" << (window ? "valid" : "null") 
                  << ") or context (" << (context ? "valid" : "null") << ")\n";
        return false;
    }

    isPlayingFlag = true;
    isPaused = false;
    return true;
}

void MC2Movie::setFrameData(uint8_t* data, int width, int height) {
    if (!data || !sdlWindow || !glContext) {
        std::cerr << "[MC2Movie] Invalid parameters in setFrameData\n";
        return;
    }

    if (SDL_GL_MakeCurrent(sdlWindow, glContext) != 0) {
        std::cerr << "[MC2Movie] Failed to make context current in setFrameData: " << SDL_GetError() << "\n";
        return;
    }

    // Frame count tracking
    static int frameCount = 0;
    frameCount++;
    if (frameCount == 1) {
        std::cout << "[MC2Movie] First frame received: " << width << "x" << height << "\n";
    }

    std::lock_guard<std::mutex> lock(textureMutex);
    
    // Initialize or update frame buffer
    if (frameWidth != width || frameHeight != height) {
        if (frameRGBData) {
            delete[] frameRGBData;
        }
        frameRGBData = new uint8_t[width * height * 4];
        frameWidth = width;
        frameHeight = height;
        
        std::cout << "[MC2Movie] Resizing texture to " << width << "x" << height << "\n";
        initializeTexture(width, height);

    }

    // Copy frame data
    memcpy(frameRGBData, data, width * height * 4);
    
    // Set pixel storage mode
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    
    // Update texture data
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, frameRGBData);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[MC2Movie] Error updating texture: 0x" << std::hex << error << std::dec << "\n";
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    frameReady = true;
}

void MC2Movie::saveOpenGLState() {
    glGetIntegerv(GL_VIEWPORT, savedState.viewport);
    savedState.blend = glIsEnabled(GL_BLEND);
    savedState.texture_2d = glIsEnabled(GL_TEXTURE_2D);
    savedState.depth_test = glIsEnabled(GL_DEPTH_TEST);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, savedState.clear_color);
    glGetIntegerv(GL_MATRIX_MODE, &savedState.matrix_mode);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &savedState.active_texture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedState.texture_binding_2d);
}

void MC2Movie::restoreOpenGLState() {
    glViewport(savedState.viewport[0], savedState.viewport[1],
               savedState.viewport[2], savedState.viewport[3]);
    
    if (savedState.blend)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
        
    if (savedState.texture_2d)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);
        
    if (savedState.depth_test)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
        
    glClearColor(savedState.clear_color[0], savedState.clear_color[1],
                 savedState.clear_color[2], savedState.clear_color[3]);
                 
    glMatrixMode(savedState.matrix_mode);
    glActiveTexture(savedState.active_texture);
    glBindTexture(GL_TEXTURE_2D, savedState.texture_binding_2d);
}

void MC2Movie::render() {
    if (!frameReady || !frameRGBData || !isPlayingFlag || isPaused) {
        return;
    }

    // Frame timing is now controlled by MP4Player::update(), so always render when called

    std::lock_guard<std::mutex> lock(textureMutex);

    static int debugFrameCount = 0;
    debugFrameCount++;

    saveOpenGLState();

    // Get window dimensions
    int windowWidth, windowHeight;
    SDL_GL_GetDrawableSize(sdlWindow, &windowWidth, &windowHeight);
    
    // Calculate proper positioning for the designated rectangle
    int rectWidth = MC2Rect.right - MC2Rect.left;
    int rectHeight = MC2Rect.bottom - MC2Rect.top;

    // Clear any previous errors
    while (glGetError() != GL_NO_ERROR) {}

    // Enable scissor test for clipping to designated area
    glEnable(GL_SCISSOR_TEST);
    // Convert from top-left origin to OpenGL bottom-left origin
    int scissorY = windowHeight - MC2Rect.bottom;
    glScissor(MC2Rect.left, scissorY, rectWidth, rectHeight);
    
    // Set up proper rendering state
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Calculate aspect-preserving scale within the rectangle
    float videoAspect = static_cast<float>(frameWidth) / frameHeight;
    float rectAspect = static_cast<float>(rectWidth) / rectHeight;
    
    float scaledWidth, scaledHeight;
    float offsetX = MC2Rect.left, offsetY = MC2Rect.top;
    
    if (videoAspect > rectAspect) {
        scaledWidth = rectWidth;
        scaledHeight = rectWidth / videoAspect;
        offsetY += (rectHeight - scaledHeight) / 2;
    } else {
        scaledHeight = rectHeight;
        scaledWidth = rectHeight * videoAspect;
        offsetX += (rectWidth - scaledWidth) / 2;
    }

    // Set up coordinate system (top-left origin) - REVERTED TO ORIGINAL
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, windowWidth, windowHeight, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Render the texture
    glEnable(GL_TEXTURE_2D);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Draw textured quad (standard coordinates)
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(offsetX, offsetY);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(offsetX + scaledWidth, offsetY);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(offsetX + scaledWidth, offsetY + scaledHeight);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(offsetX, offsetY + scaledHeight);
    glEnd();

    // Check for errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[MC2Movie] OpenGL error during render: 0x" << std::hex << error << std::dec << "\n";
    }

    // Clean up
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    
    // Restore matrices
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    restoreOpenGLState();
}

void MC2Movie::BltMovieFrame() {
    if (!frameReady || !frameRGBData) return;
    
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frameWidth, frameHeight, GL_RGBA, GL_UNSIGNED_BYTE, frameRGBData);
    glBindTexture(GL_TEXTURE_2D, 0);
    CheckGLError("MC2Movie::BltMovieFrame");
}

void MC2Movie::update() {
    if (isPlayingFlag && !isPaused) {
        render();
    }
}

void MC2Movie::end() {
    isPlayingFlag = false;
    isPaused = false;
    std::cout << "[MC2Movie] Playback ended\n";
    cleanup();
}

void MC2Movie::stop() {
    isPlayingFlag = false;
    isPaused = false;
    std::cout << "[MC2Movie] Playback stopped\n";
}

void MC2Movie::pause() {
    if (isPlayingFlag) {
        isPaused = !isPaused;
    }
}

void MC2Movie::restart() {
    isPlayingFlag = true;
    isPaused = false;
}

void MC2Movie::setFrameRate(double fps) {
    if (fps > 0) {
        frameTime = 1.0 / fps;
        std::cout << "[MC2Movie] Frame rate set to " << fps << " FPS (frame time: " << frameTime << "s)\n";
    }
}



bool MC2Movie::isPlaying() const {
    return isPlayingFlag && !isPaused;
}

std::string MC2Movie::getMovieName() const {
    return moviePath;
}

void MC2Movie::CheckGLError(const std::string& operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "[MC2Movie] OpenGL error during " << operation << ": ";
        switch (error) {
            case GL_INVALID_ENUM:
                std::cerr << "GL_INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                std::cerr << "GL_INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                std::cerr << "GL_INVALID_OPERATION";
                break;
            case GL_STACK_OVERFLOW:
                std::cerr << "GL_STACK_OVERFLOW";
                break;
            case GL_STACK_UNDERFLOW:
                std::cerr << "GL_STACK_UNDERFLOW";
                break;
            case GL_OUT_OF_MEMORY:
                std::cerr << "GL_OUT_OF_MEMORY";
                break;
            default:
                std::cerr << "Unknown error code " << error;
        }
        std::cerr << "\n";
    }
}
