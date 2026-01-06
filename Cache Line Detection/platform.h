#ifndef PLATFORM_INC
#define PLATFORM_INC

/*
	Platform detection macros.
	These should be set automatically by the compiler, but we define
	fallbacks for safety and consistency across different build systems.
*/

/* Detect platform using predefined macros */
#if defined(__APPLE__) && defined(__MACH__)
	#define PLATFORM_MACOS 1
	#define PLATFORM_NAME "macOS"
#elif defined(__linux__)
	#define PLATFORM_LINUX 1
	#define PLATFORM_NAME "Linux"
#elif defined(_WIN32) || defined(_WIN64)
	#define PLATFORM_WINDOWS 1
	#define PLATFORM_NAME "Windows"
#elif defined(__unix__)
	#define PLATFORM_UNIX 1
	#define PLATFORM_NAME "Unix"
#else
	/* Default to generic POSIX */
	#define PLATFORM_POSIX 1
	#define PLATFORM_NAME "Unknown"
#endif

/* Platform-specific includes */
#if PLATFORM_MACOS
	#include <sys/types.h>
	#include <sys/sysctl.h>
	#include <mach/mach.h>
	#include <mach/mach_time.h>
	typedef double timing_t;
#elif PLATFORM_LINUX
	#include <stdint.h>
	#include <stdio.h>
	#include <stdlib.h>
	#include <string.h>
	#include <time.h>
	#include <unistd.h>
	#include <errno.h>
	
	/* Timing structure for Linux */
	typedef struct {
		double seconds;
	} timing_t;
	
	/* clock_gettime wrapper */
	static inline double get_time_seconds(void) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		return ts.tv_sec + ts.tv_nsec / 1e9;
	}
#elif PLATFORM_WINDOWS
	/* Windows-specific includes would go here */
	#include <windows.h>
	typedef double timing_t;
#else
	/* Fallback to POSIX */
	#include <time.h>
	typedef clock_t timing_t;
#endif

/* Convenience macro for platform-specific code */
#if PLATFORM_MACOS
	#define PLATFORM_IS_MACOS 1
#else
	#define PLATFORM_IS_MACOS 0
#endif

#if PLATFORM_LINUX
	#define PLATFORM_IS_LINUX 1
#else
	#define PLATFORM_IS_LINUX 0
#endif

#if PLATFORM_WINDOWS
	#define PLATFORM_IS_WINDOWS 1
#else
	#define PLATFORM_IS_WINDOWS 0
#endif

#endif /* PLATFORM_INC */

