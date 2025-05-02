#pragma once

#include <atomic>
#include <thread>
#include <mutex>

#include "../Logger/Logger.h"
#include <atomic>
#include <mutex>
#include <string>

static constexpr double kMinPulseWidthUs = 500.0;
static constexpr double kMaxPulseWidthUs = 2500.0;
static constexpr double kMinAngle = 0.0;
static constexpr double kMaxAngle = 180.0;

class ServoHandler {
public:
    ServoHandler(int pwmChip, int pwmChannel, double frequency);
    ~ServoHandler();

    void SetAngle(double newAngle);
    void EmergencyDisableAndLock();
    void Unlock();
    bool IsLocked();

private:
    int m_pwmChip;
    int m_pwmChannel;
    std::string m_basePath;
    double m_periodNs;
    std::atomic<bool> m_locked;
    std::mutex m_mutex;

    void writeSysfs(const std::string &path, const std::string &value);
};
