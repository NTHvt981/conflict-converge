#include "app/Log.h"

#include "raylib.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace Log
{
namespace
{

std::ofstream gFile;

const char *LevelPrefix(int level)
{
    switch (level)
    {
    case LOG_TRACE:
        return "TRACE: ";
    case LOG_DEBUG:
        return "DEBUG: ";
    case LOG_INFO:
        return "INFO: ";
    case LOG_WARNING:
        return "WARNING: ";
    case LOG_ERROR:
        return "ERROR: ";
    case LOG_FATAL:
        return "FATAL: ";
    default:
        return "";
    }
}

void RouteToFileAndConsole(int level, const char *text, va_list args)
{
    char message[512];
    std::vsnprintf(message, sizeof(message), text, args);
    std::printf("%s%s\n", LevelPrefix(level), message);
    std::fflush(stdout);
    if (gFile.is_open())
    {
        gFile << LevelPrefix(level) << message << '\n';
        gFile.flush();
    }
}

}

bool Init(const char *path)
{
    Shutdown();
    if (path == nullptr || path[0] == '\0')
    {
        return false;
    }
    std::error_code ec;
    const std::filesystem::path parent = std::filesystem::path(path).parent_path();
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
    }
    gFile.open(path, std::ios::out | std::ios::trunc);
    if (!gFile.is_open())
    {
        return false;
    }
    SetTraceLogCallback(RouteToFileAndConsole);
    return true;
}

void Shutdown()
{
    SetTraceLogCallback(nullptr);
    if (gFile.is_open())
    {
        gFile.close();
    }
}

void Fatal(const char *format, ...)
{
    char message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    TraceLog(LOG_FATAL, "%s", message);
    std::exit(EXIT_FAILURE);
}

}
