#pragma once

#include <ncnn/net.h>
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>

class NeuralNetworkHandler {
public:
    // initialize with camera index and model paths, starts processing thread
    static void Initialize(const char* paramPath, const char* binPath);
    // stop processing thread
    static void Dispose();

private:
    // main loop: capture, detect, call DeadLock/AimHelper
    static void ThreadFunc();
    static std::string       names[4];
    static std::thread       worker_;
    static std::atomic<bool> running_;
    static ncnn::Net         yolo_;
    static cv::VideoCapture  cap_;
};