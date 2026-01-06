#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "cache.h"
#include "format.h"

/* Get cache line size using native macOS sysctl (M1 compatible) */
#if PLATFORM_MACOS
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

/* Get L1 cache sizes using native macOS sysctl */
static void get_l1_cache_macOS(unsigned int* l1i, unsigned int* l1d)
{
    size_t size = sizeof(unsigned int);
    
    /* Try sysctl for L1 instruction cache */
    if (sysctlbyname("hw.l1icachesize", l1i, &size, NULL, 0) != 0) {
        *l1i = 0;
    }
    
    /* Try sysctl for L1 data cache */
    size = sizeof(unsigned int);
    if (sysctlbyname("hw.l1dcachesize", l1d, &size, NULL, 0) != 0) {
        *l1d = 0;
    }
}

/* Get L2 cache size using native macOS sysctl */
static unsigned int get_l2_cache_macOS(void)
{
    unsigned int l2Size = 0;
    size_t size = sizeof(unsigned int);
    
    if (sysctlbyname("hw.l2cachesize", &l2Size, &size, NULL, 0) != 0) {
        /* Try alternative names */
        if (sysctlbyname("hw.l2 cachesize", &l2Size, &size, NULL, 0) != 0) {
            return 0;
        }
    }
    
    return l2Size;
}

/* Get L3 cache size using native macOS sysctl */
static unsigned int get_l3_cache_macOS(void)
{
    unsigned int l3Size = 0;
    size_t size = sizeof(unsigned int);
    
    /* M1 doesn't have traditional L3, try SLC */
    if (sysctlbyname("hw.l3cachesize", &l3Size, &size, NULL, 0) != 0) {
        /* Try alternative names */
        if (sysctlbyname("hw.L3cachesize", &l3Size, &size, NULL, 0) != 0) {
            return 0;
        }
    }
    
    return l3Size;
}

/* Get cache line size using native Linux sysfs */
#elif PLATFORM_LINUX
static unsigned int get_cache_line_linux(void)
{
    FILE *fp;
    char line[256];
    unsigned int cache_line = 0;
    
    /* Try to read cache line size from sysfs */
    fp = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            cache_line = (unsigned int)atoi(line);
        }
        fclose(fp);
    }
    
    return cache_line;
}

static unsigned int get_cache_size_linux_sysfs(int level, int type)
{
    FILE *fp;
    char path[256];
    char line[256];
    unsigned int cache_size = 0;
    
    /* Build path for specific cache level and type */
    /* type: 0 = unified, 1 = instruction, 2 = data */
    const char* type_str[] = {"", "index1", "index2"};
    snprintf(path, sizeof(path), 
             "/sys/devices/system/cpu/cpu0/cache/%s/size", type_str[type]);
    
    fp = fopen(path, "r");
    if (fp) {
        if (fgets(line, sizeof(line), fp)) {
            /* Size is usually in KB, may have K suffix */
            char* end;
            cache_size = (unsigned int)strtoul(line, &end, 10);
            if (*end == 'K' || *end == 'k') {
                cache_size *= 1024;
            }
        }
        fclose(fp);
    }
    
    return cache_size;
}

static void get_l1_cache_linux(unsigned int* l1i, unsigned int* l1d)
{
    *l1i = get_cache_size_linux_sysfs(1, 1);  /* index1 is usually instruction */
    *l1d = get_cache_size_linux_sysfs(1, 2);  /* index2 is usually data */
}

static unsigned int get_l2_cache_linux(void)
{
    return get_cache_size_linux_sysfs(2, 0);  /* index2 is unified L2 */
}

static unsigned int get_l3_cache_linux(void)
{
    return get_cache_size_linux_sysfs(3, 0);  /* index3 is L3 (if exists) */
}
#endif

/* Print cache information with native and timing-based results */
static void print_cache_info(void)
{
    unsigned int results[4];
    
    printf("=== Cache Detection Results ===\n\n");
    
#if PLATFORM_MACOS
    /* Try native macOS methods first */
    unsigned int nativeL1i = 0, nativeL1d = 0;
    unsigned int nativeL2 = 0, nativeL3 = 0;
    unsigned int nativeLine = 0;
    
    get_l1_cache_macOS(&nativeL1i, &nativeL1d);
    nativeL2 = get_l2_cache_macOS();
    nativeL3 = get_l3_cache_macOS();
    nativeLine = get_cache_line_macOS();
    
    /* L1 Cache (Native) */
    printf("L1 Cache (Native macOS):\n");
    if (nativeL1i > 0 || nativeL1d > 0) {
        if (nativeL1i > 0) {
            struct size_of_data formattedL1i = unitfy_data_size(nativeL1i);
            printf("  - Instruction: %u%s\n", formattedL1i.quantity, formattedL1i.unit);
        }
        if (nativeL1d > 0) {
            struct size_of_data formattedL1d = unitfy_data_size(nativeL1d);
            printf("  - Data: %u%s\n", formattedL1d.quantity, formattedL1d.unit);
        }
    } else {
        printf("  Not available via sysctl\n");
    }
    
    /* L2 Cache (Native) */
    printf("\nL2 Cache (Native macOS):\n");
    if (nativeL2 > 0) {
        struct size_of_data formattedL2 = unitfy_data_size(nativeL2);
        printf("  - Size: %u%s\n", formattedL2.quantity, formattedL2.unit);
    } else {
        printf("  Not available via sysctl\n");
    }
    
    /* L3 Cache (Native) */
    printf("\nL3 Cache / System Level Cache (Native macOS):\n");
    if (nativeL3 > 0) {
        struct size_of_data formattedL3 = unitfy_data_size(nativeL3);
        printf("  - Size: %u%s\n", formattedL3.quantity, formattedL3.unit);
    } else {
        printf("  Not available via sysctl (M1 may use SLC instead)\n");
    }
    
    /* Cache Line (Native) */
    printf("\nCache Line (Native macOS):\n");
    if (nativeLine > 0) {
        struct size_of_data formattedLine = unitfy_data_size(nativeLine);
        printf("  - Size: %u%s\n", formattedLine.quantity, formattedLine.unit);
    } else {
        printf("  Not available via sysctl\n");
    }
    
    printf("\n-----------------------------------\n\n");
    
#elif PLATFORM_LINUX
    /* Try native Linux methods using sysfs */
    unsigned int nativeL1i = 0, nativeL1d = 0;
    unsigned int nativeL2 = 0, nativeL3 = 0;
    unsigned int nativeLine = 0;
    
    get_l1_cache_linux(&nativeL1i, &nativeL1d);
    nativeL2 = get_l2_cache_linux();
    nativeL3 = get_l3_cache_linux();
    nativeLine = get_cache_line_linux();
    
    /* L1 Cache (Native) */
    printf("L1 Cache (Native Linux):\n");
    if (nativeL1i > 0 || nativeL1d > 0) {
        if (nativeL1i > 0) {
            struct size_of_data formattedL1i = unitfy_data_size(nativeL1i);
            printf("  - Instruction: %u%s\n", formattedL1i.quantity, formattedL1i.unit);
        }
        if (nativeL1d > 0) {
            struct size_of_data formattedL1d = unitfy_data_size(nativeL1d);
            printf("  - Data: %u%s\n", formattedL1d.quantity, formattedL1d.unit);
        }
    } else {
        printf("  Not available via sysfs\n");
    }
    
    /* L2 Cache (Native) */
    printf("\nL2 Cache (Native Linux):\n");
    if (nativeL2 > 0) {
        struct size_of_data formattedL2 = unitfy_data_size(nativeL2);
        printf("  - Size: %u%s\n", formattedL2.quantity, formattedL2.unit);
    } else {
        printf("  Not available via sysfs\n");
    }
    
    /* L3 Cache (Native) */
    printf("\nL3 Cache (Native Linux):\n");
    if (nativeL3 > 0) {
        struct size_of_data formattedL3 = unitfy_data_size(nativeL3);
        printf("  - Size: %u%s\n", formattedL3.quantity, formattedL3.unit);
    } else {
        printf("  Not available via sysfs (or no L3 cache)\n");
    }
    
    /* Cache Line (Native) */
    printf("\nCache Line (Native Linux):\n");
    if (nativeLine > 0) {
        struct size_of_data formattedLine = unitfy_data_size(nativeLine);
        printf("  - Size: %u%s\n", formattedLine.quantity, formattedLine.unit);
    } else {
        printf("  Not available via sysfs\n");
    }
    
    printf("\n-----------------------------------\n\n");
#endif
    
    /* Timing-based detection results */
    printf("Timing-based Detection Results:\n");
    
    get_all_cache_sizes(results);
    
    /* L1 Cache */
    struct size_of_data formattedL1 = unitfy_data_size(results[0]);
    printf("  L1 Cache: %u%s\n", formattedL1.quantity, formattedL1.unit);
    
    /* L2 Cache */
    struct size_of_data formattedL2 = unitfy_data_size(results[1]);
    printf("  L2 Cache: %u%s\n", formattedL2.quantity, formattedL2.unit);
    
    /* L3 Cache */
    struct size_of_data formattedL3 = unitfy_data_size(results[2]);
    printf("  L3 Cache / SLC: %u%s\n", formattedL3.quantity, formattedL3.unit);
    
    /* Cache Line */
    struct size_of_data formattedLine = unitfy_data_size(results[3]);
    printf("  Cache Line: %u%s\n", formattedLine.quantity, formattedLine.unit);
    
    printf("\n");
}

/* Print architecture-specific info for M1 */
static void print_m1_info(void)
{
#if PLATFORM_MACOS
    /* Get CPU type */
    size_t size = sizeof(uint32_t);
    uint32_t cputype;
    
    if (sysctlbyname("hw.cputype", &cputype, &size, NULL, 0) == 0) {
        /* CPU_TYPE_ARM64 = 0x0100000C */
        if ((cputype & 0xFF) == 12) { /* CPU subtype ARM64 */
            printf("=== Apple Silicon (M-series) Detected ===\n\n");
            printf("Note: M1 uses a unified cache architecture:\n");
            printf("  - P-cores: 192KB L1I + 128KB L1D per core\n");
            printf("  - E-cores: 128KB L1I + 64KB L1D per core\n");
            printf("  - Shared L2: 12MB (P-cores) + 4MB (E-cores)\n");
            printf("  - System Level Cache (SLC): ~8MB shared\n");
            printf("  - No traditional L3 cache\n\n");
        }
    }
#endif
}

int main(int argc, char** argv)
{
    /* Check for --quick flag for native-only output */
    int quickMode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--quick") == 0 || strcmp(argv[i], "-q") == 0) {
            quickMode = 1;
        }
    }
    
    print_m1_info();
    
    if (quickMode) {
        /* Quick mode: only show native values */
#if PLATFORM_MACOS
        unsigned int l1i = 0, l1d = 0;
        unsigned int l2 = 0, l3 = 0;
        unsigned int line = 0;
        
        get_l1_cache_macOS(&l1i, &l1d);
        l2 = get_l2_cache_macOS();
        l3 = get_l3_cache_macOS();
        line = get_cache_line_macOS();
        
        printf("Native Cache Information:\n");
        if (l1d > 0) {
            struct size_of_data f = unitfy_data_size(l1d);
            printf("  L1 Data: %u%s\n", f.quantity, f.unit);
        }
        if (l2 > 0) {
            struct size_of_data f = unitfy_data_size(l2);
            printf("  L2: %u%s\n", f.quantity, f.unit);
        }
        if (l3 > 0) {
            struct size_of_data f = unitfy_data_size(l3);
            printf("  L3/SLC: %u%s\n", f.quantity, f.unit);
        }
        if (line > 0) {
            struct size_of_data f = unitfy_data_size(line);
            printf("  Cache Line: %u%s\n", f.quantity, f.unit);
        }
#elif PLATFORM_LINUX
        unsigned int l1i = 0, l1d = 0;
        unsigned int l2 = 0, l3 = 0;
        unsigned int line = 0;
        
        get_l1_cache_linux(&l1i, &l1d);
        l2 = get_l2_cache_linux();
        l3 = get_l3_cache_linux();
        line = get_cache_line_linux();
        
        printf("Native Cache Information:\n");
        if (l1d > 0) {
            struct size_of_data f = unitfy_data_size(l1d);
            printf("  L1 Data: %u%s\n", f.quantity, f.unit);
        }
        if (l2 > 0) {
            struct size_of_data f = unitfy_data_size(l2);
            printf("  L2: %u%s\n", f.quantity, f.unit);
        }
        if (l3 > 0) {
            struct size_of_data f = unitfy_data_size(l3);
            printf("  L3: %u%s\n", f.quantity, f.unit);
        }
        if (line > 0) {
            struct size_of_data f = unitfy_data_size(line);
            printf("  Cache Line: %u%s\n", f.quantity, f.unit);
        }
#else
        printf("Quick mode not supported on this platform\n");
#endif
    } else {
        /* Full mode: show both native and timing-based results */
        print_cache_info();
    }
    
    return 0;
}

