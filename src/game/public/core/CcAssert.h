#pragma once

#include <cassert>
#include <cstdio>
#include <stacktrace>
#include <string>

// Central assertion macro: active in Debug, compiled out under NDEBUG.
// Use for invariant checks, never external input validation.

#ifndef NDEBUG
namespace cc_assert_detail
{

inline const char *BaseName(const char *path)
{
	const char *base = path;
	for (const char *c = path; *c != '\0'; ++c)
	{
		if (*c == '\\' || *c == '/')
		{
			base = c + 1;
		}
	}
	return base;
}

// Explicit depth: MSVC's default asks for 0xFFFF frames and walks the whole
// stack (797us against 1.8us in Debug for the same 12 frames).
inline void PrintTrace(const char *expression, const char *file, int line)
{
	std::fprintf(stderr, "\nCC_ASSERT failed: %s\n  at %s:%d\n", expression, file, line);
	const std::stacktrace trace = std::stacktrace::current(1, 12);
	for (const std::stacktrace_entry &entry : trace)
	{
		const std::string description = entry.description();
		const std::string source = entry.source_file();
		if (source.empty())
		{
			std::fprintf(stderr, "    %s\n", description.c_str());
		}
		else
		{
			std::fprintf(stderr, "    %s  (%s:%u)\n", description.c_str(),
						 BaseName(source.c_str()), entry.source_line());
		}
	}
	std::fflush(stderr);
}

} // namespace cc_assert_detail

#define CC_ASSERT(cond)                                                                                  \
	do                                                                                                   \
	{                                                                                                    \
		if (!(cond))                                                                                     \
		{                                                                                                \
			cc_assert_detail::PrintTrace(#cond, __FILE__, __LINE__);                                     \
		}                                                                                                \
		assert(cond);                                                                                    \
	} while (0)
#else
#define CC_ASSERT(cond) ((void)0)
#endif
