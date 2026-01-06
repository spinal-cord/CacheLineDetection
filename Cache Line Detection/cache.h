#ifndef CACHE_INC
#define CACHE_INC

/*
	The maximum is the absolute maximum size that can be returned.
	This function uses a heuristic to determine the cache line, or
	may just find a nice alignment that happens to be oddly fast.

	I recommend you set max to 1MB. Most people have cache
	lines smaller than 100kb. By most people, I mean me.
*/
unsigned int get_cache_line(unsigned int max, unsigned int stride);

/*
	Detect L1 cache size using timing-based analysis.
	Tests working set sizes from 16KB to 512KB range.
*/
unsigned int get_l1_cache(void);

/*
	Detect L2 cache size using timing-based analysis.
	Tests working set sizes from 256KB to 16MB range.
*/
unsigned int get_l2_cache(void);

/*
	Detect L3 cache size using timing-based analysis.
	Tests working set sizes from 4MB to 64MB range.
	Note: M1 doesn't have traditional L3, will detect System Level Cache.
*/
unsigned int get_l3_cache(void);

/*
	Get all cache sizes (L1, L2, L3, cache line) in one call.
	Results stored in the provided array:
	- results[0] = L1 cache size
	- results[1] = L2 cache size
	- results[2] = L3 cache size (or SLC on M1)
	- results[3] = cache line size
*/
void get_all_cache_sizes(unsigned int results[4]);

#endif

