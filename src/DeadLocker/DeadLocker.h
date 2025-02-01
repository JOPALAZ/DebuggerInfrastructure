#include <thread>
#include <atomic>

struct gpiod_line;

class DeadLocker
{
private:
    static gpiod_line* ButtonLine;
    static std::atomic<bool> cycle;
    static void threadFunc();
    static std::thread thrd;
    static double UnlockDelayMs;
public:
    DeadLocker() = delete;
    ~DeadLocker() = delete;
    static void Initialize(int lineOffset, double UnlockDelayMs = 1000);
    static void Dispose();
};
