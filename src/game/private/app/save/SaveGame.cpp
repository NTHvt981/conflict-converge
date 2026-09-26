#include "app/save/SaveGame.h"

#include <cstdio>
#include <filesystem>
#include <string>

std::string SaveSlotPath(int slot)
{
    if (slot < 1)
    {
        slot = 1;
    }
    if (slot > 3)
    {
        slot = 3;
    }
    return "data/slot" + std::to_string(slot) + ".ccpb";
}

std::string ReplayFramePath(const std::string &dir, int index)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "replay_%04d.ccpb", index < 0 ? 0 : index);
    return dir + "/" + buf;
}

int ReplayFrameCount(const std::string &dir)
{
    int count = 0;
    std::error_code ec;
    while (count < kReplayMaxFrames)
    {
        if (!std::filesystem::exists(ReplayFramePath(dir, count), ec) || ec)
        {
            break;
        }
        ++count;
    }
    return count;
}
