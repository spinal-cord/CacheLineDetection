#include "cache.h"
#include "fast_math.h"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#ifdef PLATFORM_MACOS
#include <mach/mach_time.h>
typedef double timing_t;
#else
#include <time.h>
typedef clock_t timing_t;
#endif

/*
	This function is basically manually code generated.
	DON'T TOUCH UNLESS YOU UNDERSTAND WHAT AN OPCODE IS.

	See: http://igoro.com/archive/gallery-of-processor-cache-effects/
*/
static void iterate_through_data(char* data, unsigned int dataSize, unsigned int stride)
{
	static const unsigned int steps = 128*1024*1024; /* Increased for L3 cache testing. */

	unsigned int lengthMod = dataSize - 1;
	unsigned int i;

	assert(is_power_of_two(dataSize));

	/*
		NOTE:
			If n is "steps", and m is maxAlignment, then this code is proportional
			to O(n log m). Therefore, be careful about increasing the step size to
			fix this assert. Try decreasing the maxAlignment instead. Or "max", as
			external code knows it by.
	*/
	assert(dataSize < steps);

	for(i = 0; i < steps; ++i)
		++data[(i * stride) & lengthMod];
}

/* Cross-platform timing abstraction */
#ifdef PLATFORM_MACOS
static double get_time_diff_nanos(void)
{
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    return (double)timebase.numer / (double)timebase.denom;
}

static timing_t timed_iteration(char* data, unsigned int dataSize, unsigned int stride)
{
    uint64_t begin, end;
    double time_diff;
    
    begin = mach_absolute_time();
    iterate_through_data(data, dataSize, stride);
    end = mach_absolute_time();
    
    time_diff = (double)(end - begin) * get_time_diff_nanos();
    return time_diff;
}
#else
static timing_t timed_iteration(char* data, unsigned int dataSize, unsigned int stride)
{
	clock_t begin, end;

	begin = clock();
	iterate_through_data(data, dataSize, stride);
	end = clock();

	return end - begin;
}
#endif

static void fill_timing_data(
		timing_t* timingData,
		unsigned int timingDataLength,
		unsigned int maxAlignment,
		unsigned int stride
	)
{
	unsigned int currentAlignment;
	unsigned int i;

	char* targetArray = NULL;

	for(currentAlignment = 1, i = 0; currentAlignment < maxAlignment; currentAlignment *= 2, ++i)
	{
		targetArray = realloc(targetArray, currentAlignment);
		timingData[i] = timed_iteration(targetArray, currentAlignment, stride);
	}

	free(targetArray);
}

/* There's probably a magical math formula involving logarithms that should be used here, but I'm too tired to find it. */
static unsigned int determine_size_of_timing_data_required(unsigned int maxAlignment)
{
	unsigned int size, currentAlignment;

	for(
		size = 0, currentAlignment = 1;
		currentAlignment < maxAlignment;
		++size, currentAlignment *= 2
	);

	return size;
}

/*
	This function essentially finds the biggest "jump" in timings.
	A decent heuristic - it worked on my machine!
*/
static unsigned int get_cache_line_size_from_timing_data(
		timing_t* timingData,
		unsigned int numberOfDataPoints
	)
{
	unsigned int i;
	timing_t delta;

	timing_t biggestJumpAmount = -9001;	/* It's under(?) 9000! */

	unsigned int locationOfBiggestJumpAmount = 0;

	/* We ignore the first point. You can't get a delta from just one point! */
	for(i = 1; i < numberOfDataPoints; ++i)
	{
		delta = timingData[i] - timingData[i - 1];

		if(delta > biggestJumpAmount)
		{
			biggestJumpAmount = delta;
			locationOfBiggestJumpAmount = i;
		}
	}

	/*
		The best timing data is at the point before the biggest jump,
		because it's at the magical boundary that's painful to access.
	*/
	return int_pow(2, locationOfBiggestJumpAmount - 1);
}

unsigned int get_cache_line(unsigned int max, unsigned int stride)
{
	unsigned int timingDataLength = determine_size_of_timing_data_required(max);
	timing_t* timingData = malloc(timingDataLength * sizeof(timing_t));

	unsigned int result;

	fill_timing_data(
		timingData,
		timingDataLength,
		max,
		stride
	);
	
	result = get_cache_line_size_from_timing_data(
		timingData, 
		timingDataLength
	);

	free(timingData);

	return result;
}

/*
	Detect actual cache line size by finding the stride where access pattern changes.
	
	When stride crosses the cache line boundary, the number of cache lines accessed
	changes fundamentally. For cache line size C:
	- Stride < C: Multiple strides access same cache line
	- Stride >= C: Each stride accesses a different cache line
	
	The cache line size is where we see the biggest change in "time per stride unit".
*/
static unsigned int detect_cache_line_size(unsigned int maxSize)
{
    /* Test specific strides - powers of 2 are most common for cache lines */
    const unsigned int candidateStrides[] = {32, 64, 128, 256};
    const unsigned int numCandidates = sizeof(candidateStrides) / sizeof(candidateStrides[0]);
    
    timing_t timings[16];
    unsigned int i;
    
    char* targetArray = malloc(maxSize);
    if (!targetArray) {
        return 64; /* Fallback to common size */
    }
    
    /* Test each candidate stride */
    for (i = 0; i < numCandidates; i++) {
        unsigned int stride = candidateStrides[i];
        
        /* Warm up */
        timed_iteration(targetArray, maxSize, stride);
        
        /* Measure */
        timings[i] = timed_iteration(targetArray, maxSize, stride);
    }
    
    free(targetArray);
    
    /* 
       Cache line detection: find where stride causes maximum time increase
       
       When stride = cache line, we transition from:
       - stride < cache line: multiple accesses per cache line
       - stride >= cache line: one access per cache line
       
       This causes a timing jump proportional to the cache line crossing.
       
       Calculate "time increase per stride unit" for each stride transition:
       - From 32 to 64: expected jump if cache line > 64
       - From 64 to 128: expected jump if cache line > 64, < 128, or = 128
       - From 128 to 256: expected jump if cache line = 128
    */
    
    /* Find the stride transition with biggest relative time increase */
    timing_t biggestJump = 0;
    unsigned int cacheLineSize = 64; /* Default */
    
    for (i = 1; i < numCandidates; i++) {
        timing_t timeAtSmallerStride = timings[i-1];
        timing_t timeAtLargerStride = timings[i];
        unsigned int smallerStride = candidateStrides[i-1];
        unsigned int largerStride = candidateStrides[i];
        
        if (timeAtSmallerStride > 0) {
            /* Relative time increase when doubling stride */
            timing_t relativeJump = (timeAtLargerStride - timeAtSmallerStride) / timeAtSmallerStride;
            
            /* Time should roughly double when stride doubles (more cache line loads)
               But if stride is still within cache line, time increase is smaller */
            
            /* Find the stride where this ratio is closest to 1.0 (linear scaling)
               This indicates we've crossed the cache line boundary */
            timing_t ratioToLinear = (relativeJump > 1.0) ? 
                (relativeJump - 1.0) : (1.0 - relativeJump);
            
            /* When stride doubles and time roughly doubles, we've crossed cache line */
            if (ratioToLinear < biggestJump || biggestJump == 0) {
                biggestJump = ratioToLinear;
                /* The smaller stride is likely the cache line size */
                cacheLineSize = smallerStride;
            }
        }
    }
    
    /* 
       Alternative: The cache line is where time increases most when stride increases.
       When stride < cache line: time increases slowly (same cache line accesses)
       When stride crosses cache line: time jumps (new cache line per access)
    */
    timing_t maxAbsJump = 0;
    unsigned int jumpStride = 64;
    
    for (i = 1; i < numCandidates; i++) {
        timing_t absJump = timings[i] - timings[i-1];
        if (absJump > maxAbsJump) {
            maxAbsJump = absJump;
            jumpStride = candidateStrides[i-1];
        }
    }
    
    /* The stride at the jump boundary is the cache line size */
    if (maxAbsJump > 0) {
        cacheLineSize = jumpStride;
    }
    
    return cacheLineSize;
}

/*
	Detect cache size at a specific level using timing analysis.
	This function tests different working set sizes and finds the point
	where access time increases significantly (cache boundary).
	
	minSize: Minimum size to test (in bytes)
	maxSize: Maximum size to test (in bytes)
	stride: Access stride - should match cache line size
	
	Returns: The cache size (the size that just fits in the cache,
	             not the size that exceeds it)
*/
static unsigned int detect_cache_level(unsigned int minSize, unsigned int maxSize, unsigned int stride)
{
	unsigned int timingDataLength;
	timing_t* timingData;
	unsigned int currentSize;
	unsigned int i;
	unsigned int numTests;
	
	/* Calculate number of test sizes within the range */
	numTests = 0;
	for (currentSize = minSize; currentSize <= maxSize; currentSize *= 2) {
		numTests++;
	}
	
	timingDataLength = numTests;
	timingData = malloc(timingDataLength * sizeof(timing_t));
	
	if (!timingData) {
		return 0;
	}
	
	/* Fill timing data for each test size */
	i = 0;
	for (currentSize = minSize; currentSize <= maxSize; currentSize *= 2, i++) {
		char* targetArray = malloc(currentSize);
		if (!targetArray) {
			free(timingData);
			return 0;
		}
		
		/* Warm up the cache */
		timed_iteration(targetArray, currentSize, stride);
		timed_iteration(targetArray, currentSize, stride);
		
		/* Measure access time */
		timingData[i] = timed_iteration(targetArray, currentSize, stride);
		
		free(targetArray);
	}
	
	/* Find the biggest jump in timing (cache boundary) */
	timing_t biggestJump = 0;
	unsigned int jumpLocation = 0;
	
	for (i = 1; i < numTests; i++) {
		timing_t delta = timingData[i] - timingData[i - 1];
		if (delta > biggestJump) {
			biggestJump = delta;
			jumpLocation = i;
		}
	}
	
	free(timingData);
	
	/* Return the size at the point BEFORE the jump (just fits in cache)
	   The jump happens when we exceed the cache size, so the previous
	   size is what fits in the cache */
	if (jumpLocation > 0) {
		unsigned int size = minSize;
		for (i = 1; i < jumpLocation; i++) {
			size *= 2;
		}
		return size;
	}
	
	/* If no significant jump found, estimate based on timing pattern
	   Look for the first size where timing exceeds baseline by > 50% */
	if (numTests >= 2 && timingData[0] > 0) {
		for (i = 1; i < numTests; i++) {
			if (timingData[i] > timingData[0] * 1.5) {
				unsigned int size = minSize;
				unsigned int j;
				for (j = 1; j < i; j++) {
					size *= 2;
				}
				return size;
			}
		}
	}
	
	/* Fallback: return estimated cache size based on max tested */
	return maxSize / 2;
}

/*
	Detect L1 cache size.
	On M1: P-cores have 128KB L1D, E-cores have 64KB L1D
*/
unsigned int get_l1_cache(void)
{
	/* First detect cache line size, then use it for stride */
	unsigned int cacheLine = detect_cache_line_size(1 * 1024 * 1024);
	/* Test sizes from 16KB to 512KB to cover typical L1 sizes */
	return detect_cache_level(16 * 1024, 512 * 1024, cacheLine);
}

/*
	Detect L2 cache size.
	On M1: Shared L2 is 12MB for P-cores, 4MB for E-cores
*/
unsigned int get_l2_cache(void)
{
	/* First detect cache line size, then use it for stride */
	unsigned int cacheLine = detect_cache_line_size(1 * 1024 * 1024);
	/* Test sizes from 256KB to 16MB to cover typical L2 and M1 shared L2 */
	return detect_cache_level(256 * 1024, 16 * 1024 * 1024, cacheLine);
}

/*
	Detect L3 cache size.
	Note: M1 doesn't have traditional L3 cache - instead has System Level Cache (SLC).
	We'll detect what's available in the 4MB-64MB range.
*/
unsigned int get_l3_cache(void)
{
	/* First detect cache line size, then use it for stride */
	unsigned int cacheLine = detect_cache_line_size(1 * 1024 * 1024);
	/* Test sizes from 4MB to 64MB to cover L3 and M1 SLC */
	return detect_cache_level(4 * 1024 * 1024, 64 * 1024 * 1024, cacheLine);
}

/*
	Get all cache sizes in one call.
*/
void get_all_cache_sizes(unsigned int results[4])
{
	/* First detect cache line size to use as stride for other detections */
	unsigned int cacheLine = detect_cache_line_size(1 * 1024 * 1024);
	
	results[0] = detect_cache_level(16 * 1024, 512 * 1024, cacheLine);  /* L1 */
	results[1] = detect_cache_level(256 * 1024, 16 * 1024 * 1024, cacheLine);  /* L2 */
	results[2] = detect_cache_level(4 * 1024 * 1024, 64 * 1024 * 1024, cacheLine);  /* L3/SLC */
	results[3] = cacheLine;  /* Cache line */
}

