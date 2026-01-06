#include <stdio.h>
#include <stdlib.h>

#ifdef PLATFORM_MACOS
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "cache.h"
#include "format.h"

/* Get cache line size using native macOS sysctl (M1 compatible) */
#ifdef PLATFORM_MACOS
static unsigned int get_cache_line_macOS(void)
{
    int mib[2];
    size_t lineSize = sizeof(unsigned int);
    unsigned int cacheLine;
    int result;
    
    mib[0] = CTL_HW;
    mib[1] = HW_CACHELINE;
    
    result = sysctl(mib, 2, &cacheLine, &lineSize, NULL, 0);
    if (result == 0) {
        return cacheLine;
    }
    
    /* Try sysctlbyname as fallback */
    lineSize = sizeof(cacheLine);
    result = sysctlbyname("hw.cachelinesize", &cacheLine, &lineSize, NULL, 0);
    if (result == 0) {
        return cacheLine;
    }
    
    return 0;
}
#endif

/* Inspired by: http://igoro.com/archive/gallery-of-processor-cache-effects/ */
int main(int argc, char** argv)
{
	unsigned int cacheLine;
	
#ifdef PLATFORM_MACOS
	/* Try native macOS method first (fast and accurate for M1) */
	unsigned int nativeResult = get_cache_line_macOS();
	if (nativeResult > 0) {
		cacheLine = nativeResult;
	} else {
		/* Fall back to timing-based detection */
		cacheLine = get_cache_line(1*1024*1024);
	}
#else
	/* One megabyte. Surely our cache is smaller than that! */
	cacheLine = get_cache_line(1*1024*1024);
#endif

	struct size_of_data formattedResult = unitfy_data_size(cacheLine);

	printf("Cache line -> [%u%s]\n", formattedResult.quantity, formattedResult.unit);

	/* My cache line is 64kb.. what's yours? */
	if(cacheLine == 65536)
		printf("\nHeeeey, that's my cache line too! <3\n");

	printf("\n");

	return 0;
}

