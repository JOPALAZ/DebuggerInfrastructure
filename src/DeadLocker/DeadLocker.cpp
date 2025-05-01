#include "DeadLocker.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../AimHandler/AimHandler.h"
#include "../LaserHandler/LaserHandler.h"

gpiod_line*                                         DeadLocker::ButtonLine   = nullptr;
std::thread                                         DeadLocker::thrd;
std::atomic<bool>                                   DeadLocker::cycle{false};
std::atomic<bool>                                   DeadLocker::locked{false};
double                                              DeadLocker::UnlockDelayMs = 5000;
std::set<std::string>                               DeadLocker::lockReasons;
std::mutex                                          DeadLocker::mtx;
std::unordered_map<std::string, std::thread>        DeadLocker::unlockThrdPool;
std::unordered_map<std::string, std::atomic<std::chrono::_V2::system_clock::time_point>>  DeadLocker::cancelUnlockMap;
std::unordered_map<std::string, std::atomic<bool>>  DeadLocker::resolvingMap;

void DeadLocker::Initialize(int lineOffset) {
    ButtonLine    = GPIOHandler::GetLine(lineOffset);
    GPIOHandler::RequestLineInput(ButtonLine, "EmergencyButtonGPIO");
    cycle.store(true);
    thrd = std::thread(threadFunc);
}

void DeadLocker::Dispose() {
    cycle.store(false);
    if (thrd.joinable()) thrd.join();

    for(auto& el : cancelUnlockMap)
    {
        el.second.store(std::chrono::system_clock::now());
    }
    
    GPIOHandler::ReleaseLine(ButtonLine);
}

bool DeadLocker::IsLocked() {
    return locked.load();
}

void DeadLocker::EmergencyInitiate(const std::string& caller) {
    std::lock_guard<std::mutex> lk(mtx);
    if (lockReasons.empty()) {
        locked.store(true);
        LaserHandler::EmergencyDisableAndLock();
        AimHandler::EmergencyDisableAndLock();
    }
    cancelUnlockMap[caller].store(std::chrono::system_clock::now());
    lockReasons.insert(caller);
}

void DeadLocker::Recover(const std::string& caller) {
    std::lock_guard<std::mutex> lk(mtx);
    if(lockReasons.find(caller) == lockReasons.end()) return;
    bool callerAlreadyUnlocking = DeadLocker::resolvingMap[caller].load();
    if (!callerAlreadyUnlocking) {
        unlockThrdPool[caller] = std::thread([caller] {
            std::chrono::_V2::system_clock::time_point unlockTime = std::chrono::system_clock::now();
            DeadLocker::resolvingMap[caller].store(true);
            auto total = int64_t(DeadLocker::UnlockDelayMs);
            int64_t waited = 0;
            while (waited < total && unlockTime > DeadLocker::cancelUnlockMap[caller].load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                waited += 10;
            }
            if (unlockTime > DeadLocker::cancelUnlockMap[caller].load()) 
            {
                DeadLocker::lockReasons.erase(caller);
                if(DeadLocker::lockReasons.empty())
                {
                    DeadLocker::unlockNow();
                }
            }
            DeadLocker::resolvingMap[caller].store(false);
        });
        unlockThrdPool[caller].detach();
    }
}

void DeadLocker::unlockNow() {
    std::lock_guard<std::mutex> lk(mtx);
    if (locked.load() && lockReasons.empty()) {
        locked.store(false);
        LaserHandler::Unlock();
        AimHandler::Unlock();
        AimHandler::RestoreLastState();
    }
}

void DeadLocker::threadFunc() {
    auto lastReleased = std::chrono::system_clock::now();
    while (cycle.load()) {
        if (!gpiod_line_get_value(ButtonLine)) {
            EmergencyInitiate("DeadLocker");
            GPIOHandler::WaitForValue(ButtonLine, true, cycle);
            lastReleased = std::chrono::system_clock::now();
        } else {
            auto now = std::chrono::system_clock::now();
            if (locked.load() &&
                std::chrono::duration<double, std::milli>(now - lastReleased).count()
                    > UnlockDelayMs)
            {
                Recover("DeadLocker");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
