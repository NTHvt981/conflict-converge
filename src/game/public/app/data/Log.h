#pragma once

namespace Log
{

// Truncates, creates missing parent dirs, and routes raylib logs to file +
// console. False when the file cannot be opened (console still works). Idempotent.
bool Init(const char *path);
// Closes the file and restores raylib's console-only output. Safe without Init
// and to repeat.
void Shutdown();
// Formats and emits a fatal message, then exits with EXIT_FAILURE. Never returns.
void Fatal(const char *format, ...);

} // namespace Log
