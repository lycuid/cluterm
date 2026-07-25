#ifndef __CLUTERM__DEBUG_H__
#define __CLUTERM__DEBUG_H__

#include <stdio.h>  // IWYU pragma: keep
#include <stdlib.h> // IWYU pragma: keep

#define debug(...)                                                             \
    do {                                                                       \
        printf(__VA_ARGS__);                                                   \
        fflush(stdout);                                                        \
    } while (0)

#define debug_0(...) debug(__VA_ARGS__)

#if DEBUG_LVL >= 1
#define debug_1(...) debug_0(__VA_ARGS__)
#else
#define debug_1(...) ((void)0);
#endif

#if DEBUG_LVL >= 2
#define debug_2(...) debug_0(__VA_ARGS__)
#else
#define debug_2(...) ((void)0);
#endif

#if DEBUG_LVL >= 3
#define debug_3(...) debug_0(__VA_ARGS__)
#else
#define debug_3(...) ((void)0);
#endif

#if DEBUG_LVL >= 4
#define debug_4(...) debug_0(__VA_ARGS__)
#else
#define debug_4(...) ((void)0);
#endif

#if DEBUG_LVL >= 5
#define debug_5(...) debug_0(__VA_ARGS__)
#else
#define debug_5(...) ((void)0);
#endif

#endif
