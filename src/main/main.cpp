#include <iostream>
#include <thread>
#include <vector>
#include "..\Logger\Logger.h"
#include "..\Common\DbHandler.h"
#include "..\REST\RESTapi.h"

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


int main()
{
    Logger::Initialize("", 1, 0);

    DbHandler db;
    
    RESTApi rest(&db,"0.0.0.0",8080);

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
    for(;;)
    {
        std::this_thread::sleep_for(std::chrono::seconds(600));
    }
    rest.Stop();
    return 0;
}