#include "test_metrics.h"
#include <iostream>
#include <sstream>
#include <fstream>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/imgutils.h>
}

class VideoValidator {
public:
    static VideoTestMetrics validateFile(const std::string& filepath) {
        VideoTestMetrics metrics;
        metrics.filename = filepath;

        AVFormatContext* formatCtx = nullptr;

        // Try to open the file
        int ret = avformat_open_input(&formatCtx, filepath.c_str(), nullptr, nullptr);
        if (ret < 0) {
            char error_buf[64];
            av_strerror(ret, error_buf, sizeof(error_buf));
            metrics.failure_reasons.push_back("Cannot open file: " + std::string(error_buf));
            return metrics;
        }

        // Get stream info
        ret = avformat_find_stream_info(formatCtx, nullptr);
        if (ret < 0) {
            char error_buf[64];
            av_strerror(ret, error_buf, sizeof(error_buf));
            metrics.failure_reasons.push_back("Cannot find stream info: " + std::string(error_buf));
            avformat_close_input(&formatCtx);
            return metrics;
        }

        metrics.fileValid = true;
        metrics.duration_seconds = (double)formatCtx->duration / AV_TIME_BASE;

        int videoStreamIndex = -1;
        int audioStreamIndex = -1;

        // Find video and audio streams
        for (int i = 0; i < formatCtx->nb_streams; i++) {
            AVCodecParameters* codecpar = formatCtx->streams[i]->codecpar;

            if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO && videoStreamIndex < 0) {
                videoStreamIndex = i;
                metrics.width = codecpar->width;
                metrics.height = codecpar->height;

                AVRational framerate = formatCtx->streams[i]->avg_frame_rate;
                if (framerate.num > 0 && framerate.den > 0) {
                    metrics.fps = (double)framerate.num / framerate.den;
                }

                // Calculate expected frame count
                metrics.total_frames_expected = (int)(metrics.duration_seconds * metrics.fps);
            }
            else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO && audioStreamIndex < 0) {
                audioStreamIndex = i;
            }
        }

        // Validation checks
        if (videoStreamIndex < 0) {
            metrics.failure_reasons.push_back("No video stream found");
        }
        if (audioStreamIndex < 0) {
            metrics.failure_reasons.push_back("No audio stream found");
        }
        if (metrics.fps < 10 || metrics.fps > 60) {
            metrics.failure_reasons.push_back("Suspicious frame rate: " + std::to_string(metrics.fps));
        }
        if (metrics.duration_seconds < 0.1) {
            metrics.failure_reasons.push_back("Duration too short: " + std::to_string(metrics.duration_seconds));
        }
        if (metrics.width < 100 || metrics.height < 100) {
            metrics.failure_reasons.push_back("Resolution too small: " + std::to_string(metrics.width) + "x" + std::to_string(metrics.height));
        }

        // Check for corrupted frames by scanning through the file
        int corruptFrames = scanForCorruptedFrames(formatCtx, videoStreamIndex);
        if (corruptFrames > 0) {
            metrics.failure_reasons.push_back("Found " + std::to_string(corruptFrames) + " corrupted frames");
        }

        metrics.passed = metrics.failure_reasons.empty();

        avformat_close_input(&formatCtx);
        return metrics;
    }

private:
    static int scanForCorruptedFrames(AVFormatContext* formatCtx, int videoStreamIndex) {
        if (videoStreamIndex < 0) return 0;

        AVCodecParameters* codecpar = formatCtx->streams[videoStreamIndex]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
        if (!codec) return 0;

        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) return 0;

        avcodec_parameters_to_context(codecCtx, codecpar);

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            avcodec_free_context(&codecCtx);
            return 0;
        }

        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        int corruptFrames = 0;
        int totalFrames = 0;

        // Scan through frames looking for decode errors
        while (av_read_frame(formatCtx, packet) >= 0 && totalFrames < 100) { // Limit scan to first 100 frames
            if (packet->stream_index == videoStreamIndex) {
                int ret = avcodec_send_packet(codecCtx, packet);
                if (ret < 0) {
                    corruptFrames++;
                } else {
                    ret = avcodec_receive_frame(codecCtx, frame);
                    if (ret < 0 && ret != AVERROR(EAGAIN)) {
                        corruptFrames++;
                    }
                }
                totalFrames++;
            }
            av_packet_unref(packet);
        }

        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codecCtx);

        return corruptFrames;
    }

};

// Test all your known video files
VideoTestMetrics testKnownVideos() {
    VideoTestMetrics combinedResults;
    std::vector<std::string> testFiles = {
        "data/movies/MSFT.mp4",
        "data/movies/CINEMA1.mp4",
        // Add your briefing and pilot video paths here
    };

    bool allPassed = true;
    for (const auto& file : testFiles) {
        std::cout << "Validating: " << file << std::endl;
        VideoTestMetrics result = VideoValidator::validateFile(file);

        if (!result.passed) {
            allPassed = false;
            std::cout << "FAILED: " << file << std::endl;
            for (const auto& reason : result.failure_reasons) {
                std::cout << "  - " << reason << std::endl;
            }
        } else {
            std::cout << "PASSED: " << file << " (" << result.fps << " FPS, "
                      << result.duration_seconds << "s)" << std::endl;
        }
    }

    combinedResults.passed = allPassed;
    return combinedResults;
}