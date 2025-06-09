#include "pieloader.h"
#include "unalmas_sockets.h"
#include <qdebug.h>
#include <qlogging.h>

namespace Unalmas
{

void StartPieProcess(std::stop_token stopToken,
                     const std::string& gameProjectPath,
                     std::mutex& pieMutex,
                     OUT bool& didPieFinish)
{
    qDebug() << "Creating process";

    LPCWSTR applicationName = L"C:\\Users\\andra\\source\\repos\\Unalmas\\x64\\Debug\\PIE.exe";
    STARTUPINFO startupInfo;
    ZeroMemory(&startupInfo, sizeof(startupInfo));
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo;
    ZeroMemory(&processInfo, sizeof(processInfo));

    const unsigned long long newSizeW = gameProjectPath.size() + 1;
    std::size_t convertedChars = 0;
    wchar_t* wcString = new wchar_t[newSizeW];
    mbstowcs_s(&convertedChars, wcString, newSizeW, gameProjectPath.c_str(), _TRUNCATE);

    BOOL couldCreateProcess = CreateProcess(
        applicationName,		// application name
        wcString,				// command line arguments
        NULL,					// process security attributes
        NULL,					// primary thread security attributes
        FALSE,					// handles are not inherited
        0,						// creation flags
        NULL,					// use parent's environment block
        NULL,					// use parent's starting directory
        &startupInfo,
        &processInfo);

    delete[] wcString;

    if (couldCreateProcess)
    {
        constexpr DWORD piePollDurationMS = 1000;
        bool didProcessExit { false };
        while (!didProcessExit && !stopToken.stop_requested())
        {
            const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, piePollDurationMS);
            if (waitResult == WAIT_TIMEOUT)
            {
                continue;
            }

            if (waitResult == WAIT_OBJECT_0)
            {
                didProcessExit = true;
            }
            else
            {
                // TODO
                // WaitForSingleObject failed
            }
        }

        if (!didProcessExit)
        {
            // Try to terminate child process
            qDebug() << "Terminating PIE";
            constexpr unsigned int exitCode = 1u;
            TerminateProcess(processInfo.hProcess, exitCode);
        }

        CloseHandle(processInfo.hProcess);
        CloseHandle(processInfo.hThread);

        {
            std::lock_guard<std::mutex> lock(pieMutex);
            didPieFinish = true;
        }
    }
    else
    {
        qDebug() << "Couldn't create PIE process.";
    }
}

PieLoader::~PieLoader()
{
    _pieStopSource.request_stop();
}

void PieLoader::StartPie(const std::string &gameRuntimeDll)
{
    TryTeardown();

    _didFinish = false;

    std::stop_source stopSource;
    _pieStopSource.swap(stopSource);

    _thread = std::jthread(StartPieProcess,
                           _pieStopSource.get_token(),
                           gameRuntimeDll,
                           std::ref(_pieMutex),
                           OUT std::ref(_didFinish));
}

bool PieLoader::DidFinish()
{
    std::lock_guard<std::mutex> lock(_pieMutex);
    return _didFinish;
}

std::stop_source &PieLoader::GetPieStopSource()
{
    return _pieStopSource;
}

void PieLoader::TryTeardown()
{
    if (!_thread.joinable())
    {
        return;
    }

    _pieStopSource.request_stop();
    _thread.detach();

    bool didTeardownFinish { false };
    while (!didTeardownFinish)
    {
        {
            std::lock_guard<std::mutex> lock(_pieMutex);
            didTeardownFinish = _didFinish;
        }

        if (!didTeardownFinish)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

}
