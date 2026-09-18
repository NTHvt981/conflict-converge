// Single-translation-unit implementation for the header-only rini library
// (raysan5/rini, INI-style settings reader/writer used by Menu settings).
// RINI_VALUE_DELIMITER must be '=' to match data/settings.cfg's key=value
// format (upstream default is ' '). This TU is compiled into the game binary
// via src/**.cpp and explicitly into the test/e2e runners (see premake5.lua).
#define RINI_VALUE_DELIMITER '='
#define RINI_IMPLEMENTATION
#include "rini.h"
