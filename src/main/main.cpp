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
std::mutex mtx;
std::condition_variable conditionalVar;
namespace DebuggerInfrastructure
{
    
    struct WebUI {
        std::unique_ptr<FrontEnd> front = nullptr;
        std::unique_ptr<RESTApi> rest = nullptr;
    };

    bool disposed;

    std::vector<std::pair<std::function<void()>, std::string>> coreDisposeArray =
    {
        {NeuralNetworkHandler::Dispose, NAMEOF(NeuralNetworkHandler::Dispose)},
        {DeadLocker::Dispose, NAMEOF(DeadLocker::Dispose)},
        {AimHandler::Dispose, NAMEOF(AimHandler::Dispose)},
        {LaserHandler::Dispose, NAMEOF(LaserHandler::Dispose)},
        {GPIOHandler::Dispose, NAMEOF(GPIOHandler::Dispose)},
        {DbHandler::Dispose, NAMEOF(DbHandler::Dispose)},
    };

    void DisposeCore()
    {
        if(disposed) return; 
        for(auto funNamePair : coreDisposeArray)
        {
            try
            {
                Logger::Info("Calling {}", funNamePair.second);
                funNamePair.first();
            }
            catch (const std::exception& ex)
            {
                Logger::Critical("Error during {} step: {}", funNamePair.second, ex.what());
            }
        }
        disposed = true;
    }

    void signalHandler(int signal) {
        std::cerr << "Signal " << signal << " received, disposing resources..." << std::endl;
        try
        {
            DeadLocker::EmergencyInitiate(NAMEOF(main));
            DisposeCore();
        }
        catch(std::exception& ex)
        {
            std::cerr << "Irrecoverable exception during signal [" << signal << "] hanlding. EX: " << ex.what();
        }
        catch(...)
        {
            std::cerr << "Irrecoverable unknown exception during signal [" << signal << "] hanlding.";
        }
        running = false;
    }

    void InitializeCore()
    {
        Logger::Initialize("", 1, 0);
        DbHandler::Initialize();
        GPIOHandler::Initialize("gpiochip0");
        LaserHandler::Initialize(16);
        AimHandler::Initialize();
        DeadLocker::Initialize(22);
        NeuralNetworkHandler::Initialize("./res/Model/model.ncnn.param", "./res/Model/model.ncnn.bin");
        disposed = false;
    }

    WebUI InitializeWeb()
    {
        WebUI webUi;

        try
        {
            webUi.rest = std::make_unique<RESTApi>("0.0.0.0", 8081);
            webUi.rest->Start();
        }
        catch (const std::exception& ex)
        {
            Logger::Error("Could not start REST: {}", ex.what());
            webUi.rest.reset();
        }

        try
        {
            webUi.front = std::make_unique<FrontEnd>("./res/FrontEnd/index.html", "0.0.0.0", 8080);
            webUi.front->Start();
        }
        catch (const std::exception& ex)
        {
            Logger::Error("Could not start WebUI: {}", ex.what());
            webUi.front.reset();
        }

        return webUi;
    }

    void DisposeWeb(WebUI webUi)
    {
        if(webUi.front.get() != nullptr)
        {
            webUi.front.get()->Stop();
            webUi.front.reset();
        }
        if(webUi.rest.get() != nullptr)
        {
            webUi.rest.get()->Stop();
            webUi.rest.reset();
        }
    }
}



void setupSignalHandlers() {
    std::signal(SIGINT, DebuggerInfrastructure::signalHandler);
    std::signal(SIGTERM, DebuggerInfrastructure::signalHandler);
    std::signal(SIGHUP, DebuggerInfrastructure::signalHandler);
}

int main()
{
    setupSignalHandlers();
    try {
        DebuggerInfrastructure::InitializeCore();
        auto webUi = DebuggerInfrastructure::InitializeWeb();

        std::unique_lock<std::mutex> lock(mtx);
        conditionalVar.wait(lock, [] { return !running; });

        DebuggerInfrastructure::DisposeWeb(std::move(webUi));
        DebuggerInfrastructure::DisposeCore();
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "Fatal exception: " << ex.what() << std::endl;
        exit(0xDEAD);
    } catch (...) {
        std::cerr << "Unknown fatal exception." << std::endl;
        exit(0xDEAD);
    }
    return -1;
}