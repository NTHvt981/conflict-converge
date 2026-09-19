#pragma once

// Fatal logging to console + file (see Log.h.context.md).

namespace Log
{

// Opens path for writing (truncates; creates missing parent dirs) and routes
// all raylib log messages to file + console. False when the file cannot be
// opened (console still works). Idempotent.
bool Init(const char *path);
// Closes the log file and restores raylib's default console-only output.
// Safe to call without Init and to repeat.
void Shutdown();
// Formats and emits a fatal message, then exits with EXIT_FAILURE. Never returns.
void Fatal(const char *format, ...);

} // namespace Log
