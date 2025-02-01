#include <chrono>
#include <gpiod.h>
#include "DeadLocker.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../AimHandler/AimHandler.h"
#include "../LaserHandler/LaserHandler.h"
#include "../Logger/Logger.h"



gpiod_line* DeadLocker::ButtonLine = nullptr;
std::thread DeadLocker::thrd;
std::atomic<bool> DeadLocker::cycle;
double DeadLocker::UnlockDelayMs;

void DeadLocker::Initialize(int lineOffset, double UnlockDelayMs)
{
    DeadLocker::UnlockDelayMs = UnlockDelayMs;
    ButtonLine = GPIOHandler::GetLine(lineOffset);
    GPIOHandler::RequestLineInput(ButtonLine, "EmergencyButtonGPIO");
    cycle.store(true);
    thrd = std::thread(threadFunc);
}

void DeadLocker::Dispose()
{
    cycle.store(false);
    thrd.join();
    GPIOHandler::ReleaseLine(ButtonLine);   
}

void DeadLocker::threadFunc()
{
    auto lastReleased = std::chrono::system_clock::now();
    bool locked = false;;
    while (cycle.load())
    {
        //Logger::Info("alive!!");
        if(!gpiod_line_get_value(ButtonLine))
        {
            LaserHandler::EmergencyDisableAndLock();
            AimHandler::EmergencyDisableAndLock();
            locked = true;
            GPIOHandler::WaitForValue(ButtonLine, true, cycle);
            lastReleased = std::chrono::system_clock::now();
        }
        else
        {
            if(((std::chrono::duration<double,std::milli>)(std::chrono::system_clock::now() - lastReleased)).count() > UnlockDelayMs && locked)
            {
                LaserHandler::Unlock();
                AimHandler::Unlock();
                AimHandler::RestoreLastState();
                locked = false;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
