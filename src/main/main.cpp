#include <csignal>
#include <iostream>
#include <thread>
#include <vector>
#include "../FrontEnd/FrontEnd.h"
#include "../GPIOHandler/GPIOHandler.h"
#include "../ServoHandler/ServoHandler.h"
#include "../Logger/Logger.h"
#include "../Common/DbHandler.h"
#include "../REST/RESTapi.h"
#include "../LaserHandler/LaserHandler.h"
#include "../AimHandler/AimHandler.h"
#include "../DeadLocker/DeadLocker.h"
#include "../NeuralNetworkHandler/NeuralNetworkHandler.h"

std::string generateGUID() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist32(0, 0xFFFFFFFF);
    std::uniform_int_distribution<uint16_t> dist16(0, 0xFFFF);

    uint32_t part1 = dist32(gen);
    uint16_t part2 = dist16(gen);
    uint16_t part3 = dist16(gen);
    uint16_t part4 = dist16(gen);
    uint64_t part5 = (static_cast<uint64_t>(dist16(gen)) << 48) | (dist32(gen) & 0xFFFFFFFFFFFF);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << part1 << '-'
        << std::setw(4) << part2 << '-'
        << std::setw(4) << part3 << '-'
        << std::setw(4) << part4 << '-'
        << std::setw(12) << part5;

    return oss.str();
}
void signalHandler(int signal) {
    std::cerr << "Signal " << signal << " received, disposing resources..." << std::endl;
    LaserHandler::Dispose();
    exit(signal);
}

void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGHUP, signalHandler);
}

int main()
{
    setupSignalHandlers();
    Logger::Initialize("", 1, 0);
    GPIOHandler::Initialize("gpiochip0");
    gpiod_line* LaserLine = GPIOHandler::GetLine(16);
    GPIOHandler::RequestLineOutput(LaserLine, "LaserGPIOpin");
    LaserHandler::Initialize(LaserLine);
    AimHandler::Initialize(15,14);
    DeadLocker::Initialize(22);
    DbHandler db;
    
    RESTApi rest(&db,"0.0.0.0",8081);
    FrontEnd front("./res/FrontEnd/index.html","0.0.0.0",8080);
    auto threadFunc = [&](int threadId) {
        const int insertCount = 4;
        for (int i = 0; i < insertCount; ++i)
        {
            db.InsertData(RecordData{
                static_cast<int64_t>(time(nullptr)),    // time
                threadId,                               // event
                "TEST",                                 // className
                generateGUID()                          // outcome
            });
        }
        Logger::Info("Thread {} done inserting.", threadId);
    };

    const int numThreads = 12; 
    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (int tId = 0; tId < numThreads; ++tId) {
        threads.emplace_back(threadFunc, tId);
    }

    for (auto& t : threads) {
        t.join();
    }

    Logger::Critical("TEST CRIT");
    Logger::Error("TEST ERR");
    Logger::Info("TEST INF");
    Logger::Verbose("TEST VRB"); 
    rest.Start();
    front.Start();
    NeuralNetworkHandler::Initialize("./res/Model/model.ncnn.param", "./res/Model/model.ncnn.bin");
    for(;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(600));
    }
    rest.Stop();
    front.Stop();
    LaserHandler::Dispose();
    return 0;
}
