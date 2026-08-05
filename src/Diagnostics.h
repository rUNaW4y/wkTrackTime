#ifndef WKMINE_TIMER_DIAGNOSTICS_H
#define WKMINE_TIMER_DIAGNOSTICS_H

#include <cstdarg>
#include <filesystem>

class Diagnostics {
public:
	static void initialize(const std::filesystem::path &waDir);
	static void log(const char *fmt, ...);

private:
	static void vlog(const char *fmt, va_list args);
};

#endif // WKMINE_TIMER_DIAGNOSTICS_H
