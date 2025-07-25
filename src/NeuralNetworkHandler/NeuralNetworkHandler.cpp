#include "NeuralNetworkHandler.h"
#include "../DeadLocker/DeadLocker.h"
#include "../AimHandler/AimHandler.h"
#include "../Logger/Logger.h"
#include "../DbHandler/DbHandler.h"
namespace DebuggerInfrastructure
{
    std::string NeuralNetworkHandler::names[3] = {"Person", "Pet", "Insect"};

    std::thread                                     NeuralNetworkHandler::worker_;
    std::atomic<bool>                               NeuralNetworkHandler::running_{false};
    ncnn::Net                                       NeuralNetworkHandler::yolo_;
    cv::VideoCapture                                NeuralNetworkHandler::cap_;
    std::mutex                                      NeuralNetworkHandler::frameMutex_;
    cv::Mat                                         NeuralNetworkHandler::latestFrame_;
    DbHandler*                                      NeuralNetworkHandler::dbHandler = nullptr;
    const std::chrono::duration                     shootingSustain = std::chrono::nanoseconds(1000*1000*1000);

    void NeuralNetworkHandler::Initialize(DbHandler* db , const char* paramPath, const char* binPath) {
        cap_.open("libcamerasrc af-mode=continuous ! video/x-raw,width=1024,height=1024,framerate=30/1,format=NV12 ! "
                "videoconvert ! appsink", cv::CAP_GSTREAMER);
        yolo_.opt.num_threads = 4;
        yolo_.load_param(paramPath);
        yolo_.load_model(binPath);
        running_ = true;
        worker_ = std::thread(&NeuralNetworkHandler::ThreadFunc);
        dbHandler = db;
    }

    void NeuralNetworkHandler::Dispose() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
        cap_.release();
    }

    void NeuralNetworkHandler::ThreadFunc() {
        const float mean_vals[3] = {0.f, 0.f, 0.f};
        const float norm_vals[3] = {1 / 255.f, 1 / 255.f, 1 / 255.f};
        const float score_threshold = 0.40f;
        bool needsResolving = false;
        cv::Scalar boxColor(0, 255, 0);
        cv::Scalar textColor(0, 0, 255);
        int fontFace = cv::FONT_HERSHEY_SIMPLEX;
        double fontScale = 0.5;
        int thickness = 1;
        int clsId = -1;

        cv::Mat frame;

        while (running_) {
            if (!cap_.read(frame) || frame.empty()) continue;
            cv::flip(frame, frame, -1);

            int ih = frame.rows, iw = frame.cols;
            int len = std::max(ih, iw);
            float scale = float(len) / 512.f;

            cv::Mat square(len, len, CV_8UC3, cv::Scalar(0, 0, 0));
            frame.copyTo(square(cv::Rect(0, 0, iw, ih)));

            cv::Mat rgb;
            cv::cvtColor(square, rgb, cv::COLOR_BGR2RGB);

            ncnn::Mat in = ncnn::Mat::from_pixels_resize(
                rgb.data, ncnn::Mat::PIXEL_RGB, len, len, 512, 512);
            in.substract_mean_normalize(mean_vals, norm_vals);

            ncnn::Extractor ex = yolo_.create_extractor();
            if (ex.input("in0", in) != 0) continue;

            ncnn::Mat out;
            if (ex.extract("out0", out) != 0) continue;

            int num_channels = out.h;
            int num_boxes = out.w;

            bool emergency = false;
            bool aim = false;
            float aimX = 0.f, aimY = 0.f;

            std::vector<std::tuple<cv::Rect, std::string>> drawnBoxes;

            for (int j = 0; j < num_boxes; j++) {
                const float* r0 = out.row(0);
                const float* r1 = out.row(1);
                const float* r2 = out.row(2);
                const float* r3 = out.row(3);
                float cx = r0[j], cy = r1[j], w = r2[j], h = r3[j];

                int best_cls = -1;
                float best_score = 0.f;
                for (int c = 4; c < num_channels; c++) {
                    float s = out.row(c)[j];
                    if (s > best_score) {
                        best_score = s;
                        best_cls = c - 4;
                    }
                }
                if (best_score < score_threshold) continue;

                float x0 = (cx - w * 0.5f) * scale;
                float y0 = (cy - h * 0.5f) * scale;
                float x1 = (cx + w * 0.5f) * scale;
                float y1 = (cy + h * 0.5f) * scale;

                aimX = cx / 512.f;
                aimY = cy / 512.f;

                if (best_cls >= 0 && best_cls < 3) {
                    cv::Rect box = cv::Rect(cv::Point(int(x0), int(y0)), cv::Point(int(x1), int(y1)));
                    drawnBoxes.emplace_back(box, names[best_cls]);

                    if (best_cls <= 1) {
                        emergency = true;
                        aim = false;
                        clsId = best_cls;
                        break;
                    }

                    aim = true;
                    clsId = best_cls;
                }
            }

            std::string name = clsId >= 0 && clsId <3? names[clsId] : "UNKNOWN";
            
            if (emergency) {
                std::string msg = fmt::format("Protected entity was detected: {}: X({}) Y({})", name, aimX, aimY);
                Logger::Info(msg);
                if(!DeadLocker::lockReasons.contains(NAMEOF(NeuralNetworkHandler)))
                {
                    dbHandler->InsertDataNow(EMERGENCYADDLOCKREASON, NAMEOF(NeuralNetworkHandler), msg);
                    DeadLocker::EmergencyInitiate(NAMEOF(NeuralNetworkHandler));
                    needsResolving = true;
                }
            } else if(!AimHandler::IsCalibrationEnabled()) {
                if (DeadLocker::IsLocked() && DeadLocker::lockReasons.contains(NAMEOF(NeuralNetworkHandler)) && needsResolving) {
                    dbHandler->InsertDataNow(EMERGENCYREMOVELOCKREASON, NAMEOF(NeuralNetworkHandler), "All protected entities exited the camera view");
                    DeadLocker::Recover(NAMEOF(NeuralNetworkHandler));
                    needsResolving = false;
                } else if (aim && !DeadLocker::IsLocked()) {
                    std::string msg = fmt::format("An {} was detected at X({}) Y({}). Eliminating.", name, aimX, aimY);
                    if(std::chrono::_V2::system_clock::now() - AimHandler::GetLastShoot() > shootingSustain)
                    {
                        dbHandler->InsertDataNow(ELIMINATION, NAMEOF(NeuralNetworkHandler), msg);
                    }
                    std::string response = AimHandler::ShootAt({aimX, aimY});
                    Logger::Info(response);
                } else if(std::chrono::_V2::system_clock::now() - AimHandler::GetLastShoot() > shootingSustain && AimHandler::IsLaserEnabled()) {
                    std::string response = AimHandler::Disarm();
                    Logger::Info(response);
                }
            }

            cv::Mat drawn = frame.clone();
            for (const auto& [box, label] : drawnBoxes) {
                cv::rectangle(drawn, box, boxColor, 2);
                cv::putText(drawn, label, box.tl(), fontFace, fontScale, textColor, thickness);
            }

            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                latestFrame_ = std::move(drawn);
            }
        }
    }
}