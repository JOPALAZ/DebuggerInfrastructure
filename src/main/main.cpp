#include <csignal>
#include <iostream>
#include <thread>
#include <vector>
#include "../FrontEnd/FrontEnd.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../ServoHandler/ServoHandler.h"
#include "../Logger/Logger.h"
#include "../DbHandler/DbHandler.h"
#include "../REST/RESTapi.h"
#include "../LaserHandler/LaserHandler.h"
#include "../AimHandler/AimHandler.h"
#include "../DeadLocker/DeadLocker.h"
#include "../NeuralNetworkHandler/NeuralNetworkHandler.h"

bool running = true;
void signalHandler(int signal) {
    std::cerr << "Signal " << signal << " received, disposing resources..." << std::endl;
    try
    {
        DeadLocker::EmergencyInitiate(NAMEOF(main));
        NeuralNetworkHandler::Dispose();
        LaserHandler::Dispose();
        AimHandler::Dispose();
        DeadLocker::Dispose();
        GPIOHandler::Dispose();
    }
    catch(std::exception& ex)
    {
        std::cerr << "Irrecoverable exception during signal [" << signal << "] hanlding. EX: " << ex.what();
        exit(signal);
    }
    catch(...)
    {
        std::cerr << "Irrecoverable unknown exception during signal [" << signal << "] hanlding.";
        exit(signal);
    }
    running = false;
}

void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGHUP, signalHandler);
}

int main()
{
    try {
        setupSignalHandlers();
        DbHandler* DbHandlerPtr = new DbHandler();
        Logger::Initialize("", 1, 0);
        GPIOHandler::Initialize("gpiochip0");
        gpiod_line* LaserLine = GPIOHandler::GetLine(16);
        GPIOHandler::RequestLineOutput(LaserLine, "LaserGPIOpin");
        LaserHandler::Initialize(LaserLine);
        AimHandler::Initialize(DbHandlerPtr);
        DeadLocker::Initialize(DbHandlerPtr, 22);
        NeuralNetworkHandler::Initialize(DbHandlerPtr, "./res/Model/model.ncnn.param", "./res/Model/model.ncnn.bin");
        RESTApi rest(DbHandlerPtr,"0.0.0.0",8081);
        FrontEnd front("./res/FrontEnd/index.html","0.0.0.0",8080);
        rest.Start();
        front.Start();
        while(running)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        rest.Stop();
        front.Stop();
        if(DbHandlerPtr)
        {
            delete DbHandlerPtr;
            DbHandlerPtr = nullptr;
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Fatal exception: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal exception." << std::endl;
        return 2;
    }
}
