#ifndef PIELOADER_H
#define PIELOADER_H

#include <mutex>
#include <stop_token>
#include <thread>

namespace Unalmas
{

class PieLoader
{
public:
    ~PieLoader();

    void StartPie(const std::string& gameRuntimeDll);
    bool DidFinish();

private:
    std::mutex _pieMutex;
    bool _didFinish { false };
    std::stop_source _pieStopSource;
    std::jthread _thread;

    void TryTeardown();
};

} // namespace Unalmas
#endif // PIELOADER_H
