#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>
#include <version>

// C++23 floor: the first-party projects are pinned to cppdialect "C++23" and
// prj/Directory.Build.props selects the newest installed MSVC toolset. These
// guards report a toolset that fell back to an older default once, here, rather
// than at each feature site.
#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202211L
#error "C++23 <expected> needs MSVC 14.40+; run premake5 vs2022 to regenerate the toolset pin"
#endif
#if !defined(__cpp_lib_stacktrace) || __cpp_lib_stacktrace < 202011L
#error "C++23 <stacktrace> needs MSVC 14.40+; run premake5 vs2022 to regenerate the toolset pin"
#endif
#if !defined(__cpp_lib_ranges_enumerate) || __cpp_lib_ranges_enumerate < 202302L
#error "C++23 views::enumerate needs MSVC 14.44+; run premake5 vs2022 to regenerate the toolset pin"
#endif

#include <glm/glm.hpp>
#include <RmlUi/Core.h>
#include "raylib.h"
