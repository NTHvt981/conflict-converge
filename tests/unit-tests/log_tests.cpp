// Unit tests for fatal logging (Log.h). Fatal itself exits the process
// (raylib LOG_FATAL semantics), so it cannot run in-process here; these
// cover Init/Shutdown plus the file+console routing through a non-fatal
// level, which exercises the same callback Fatal uses.

#include "test_harness.h"

#include "app/data/Log.h"
#include "raylib.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{

std::string ReadWholeFile(const std::string &path)
{
    std::string out;
    std::ifstream f(path);
    if (f)
    {
        std::string line;
        while (std::getline(f, line))
        {
            out += line;
            out += '\n';
        }
    }
    return out;
}

} // namespace

void RunLogTests()
{
    // --- null/empty path refuses without crashing; shutdown is safe bare ---
    Log::Shutdown();
    CC_CHECK(!Log::Init(nullptr));
    CC_CHECK(!Log::Init(""));
    Log::Shutdown();

    // --- init routes raylib messages to the file (same path Fatal uses) ---
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "cc_log_tests";
    const std::string path = (dir / "nested" / "game.log").string();
    std::filesystem::remove_all(dir);
    CC_CHECK(Log::Init(path.c_str()));
    TraceLog(LOG_WARNING, "disk %d%% full", 99);
    Log::Shutdown();
    CC_CHECK(ReadWholeFile(path).find("WARNING: disk 99% full") != std::string::npos);

    // --- re-init swaps files; repeated shutdown stays safe ---
    const std::string second = (dir / "second.log").string();
    CC_CHECK(Log::Init(path.c_str()));
    CC_CHECK(Log::Init(second.c_str()));
    TraceLog(LOG_ERROR, "boom");
    Log::Shutdown();
    Log::Shutdown();
    CC_CHECK(ReadWholeFile(path).find("boom") == std::string::npos);
    CC_CHECK(ReadWholeFile(second).find("ERROR: boom") != std::string::npos);

    std::filesystem::remove_all(dir);
}
