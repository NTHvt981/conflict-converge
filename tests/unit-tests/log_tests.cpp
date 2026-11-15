// Unit tests for Log.h. Fatal exits the process, so these cover Init/Shutdown
// plus the file+console routing shared with Fatal, via a non-fatal level.

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
	Log::Shutdown();
	CC_CHECK(!Log::Init(nullptr));
	CC_CHECK(!Log::Init(""));
	Log::Shutdown();

	const std::filesystem::path dir =
		std::filesystem::temp_directory_path() / "cc_log_tests";
	const std::string path = (dir / "nested" / "game.log").string();
	std::filesystem::remove_all(dir);
	CC_CHECK(Log::Init(path.c_str()));
	TraceLog(LOG_WARNING, "disk %d%% full", 99);
	Log::Shutdown();
	CC_CHECK(ReadWholeFile(path).find("WARNING: disk 99% full") != std::string::npos);

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
