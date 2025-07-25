#pragma once

#include <ncnn/net.h>
#include <opencv2/opencv.hpp>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <chrono>
namespace DebuggerInfrastructure
{
    class DbHandler;

    class NeuralNetworkHandler {
    public:
        static void Initialize(DbHandler* db, const char* paramPath, const char* binPath);
        static void Dispose();

        static inline cv::Mat GetLatestFrame()
        {
            std::lock_guard<std::mutex> lock(frameMutex_);
            return latestFrame_.clone();
        }

    private:
        static void ThreadFunc();

        static std::string                                 names[3];
        static std::thread                                 worker_;
        static std::atomic<bool>                           running_;
        static ncnn::Net                                   yolo_;
        static cv::VideoCapture                            cap_;
        static std::mutex                                  frameMutex_;
        static cv::Mat                                     latestFrame_;
        static DbHandler*                                  dbHandler;
    };
}