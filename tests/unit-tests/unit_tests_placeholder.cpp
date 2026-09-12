// Placeholder TU so the unit-tests filter has a compilable file.
// Real unit tests (collision, pathfinding, resources, damage, events) arrive in M7.
// The include + static_assert below prove the glm include path works for tests.

#include "MathUtils.h"

static_assert(sizeof(cc::Vec2) == 8, "cc::Vec2 must be two floats");
static_assert(cc::TILE_SIZE == 64.0f, "tile grid must be 64x64");

inline void ConflictConverge_UnitTests_Placeholder()
{
}
