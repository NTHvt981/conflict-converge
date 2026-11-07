#pragma once

#include "app/data/UnitConfig.h"
#include "units/Unit.h"

// Hardcoded baseline for `type`; used when a type has no file and as the
// per-field fallback for tolerated missing keys.
UnitConfig DefaultUnitConfig(UnitType type);
