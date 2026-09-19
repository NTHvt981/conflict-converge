#pragma once

// Fatal logging to console + file. Thin wrapper over raylib's TraceLog:
// the game links raylib in every binary (game, headless tests, e2e), so no
// new dependency. Console output works as-is (all premake projects are
// ConsoleApp); file output needs Init to install a raylib log callback that
// tees each message to both. Without Init, Fatal still prints to stdout.
// Fatal always exits the process (raylib LOG_FATAL semantics), so it is for
// unrecoverable errors only and cannot be called from unit tests.

namespace Log
{

// Opens path for writing (truncates; missing parent dirs are created) and
// routes all raylib log messages to file + console from here on. Returns
// false when the file cannot be opened (console logging still works).
// Idempotent: re-inits close the previous file first.
bool Init(const char *path);
// Closes the log file and restores raylib's default console-only output.
// Safe to call without Init and to repeat.
void Shutdown();
// Formats and emits a fatal message (file + console when Init ran,
// console otherwise), then exits with EXIT_FAILURE. Never returns.
void Fatal(const char *format, ...);

} // namespace Log
