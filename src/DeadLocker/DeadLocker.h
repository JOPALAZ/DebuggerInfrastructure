#pragma once
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <gpiod.h>
#include <unordered_map>
#include "../Logger/Logger.h"

namespace DebuggerInfrastructure
{
    class DbHandler;

    class DeadLocker {
    public:
        // lineOffset - GPIO line for emergency button
        // unlockDelayMs - delay after "Recover" before final unlock
        static void Initialize(DbHandler* db, int lineOffset);
        static void Dispose();
        static bool IsLocked();
        static void EmergencyInitiate(const std::string& caller);
        static void Recover(const std::string& caller);
        static std::set<std::string> lockReasons;

    private:
        static void threadFunc();
        static void unlockNow();

        static gpiod_line* ButtonLine;
        static DbHandler* dbHandler;
        static std::thread thrd;
        static std::atomic<bool> cycle;
        static std::atomic<bool> locked;
        static double UnlockDelayMs;
        static std::mutex mtx;
        static std::unordered_map<std::string, std::thread>       unlockThrdPool;
        static std::unordered_map<std::string, std::atomic<std::chrono::_V2::system_clock::time_point>> cancelUnlockMap;
        static std::unordered_map<std::string, std::atomic<bool>> resolvingMap;
    };
}