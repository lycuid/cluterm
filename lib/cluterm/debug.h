#ifndef __CLUTERM__DEBUG_H__
#define __CLUTERM__DEBUG_H__

#include <stdio.h>  // IWYU pragma: keep
#include <stdlib.h> // IWYU pragma: keep

#ifndef DEBUG_LVL
#define DEBUG_LVL 0
#endif

#ifndef DEBUG_SINK
#define DEBUG_SINK stderr
#endif

#define debug_var __attribute__((unused))

#define debug(...)                                                             \
    do {                                                                       \
        fprintf(DEBUG_SINK, __VA_ARGS__);                                      \
        fflush(DEBUG_SINK);                                                    \
    } while (0)

#define debug_0(...)                                                           \
    do {                                                                       \
        fprintf(DEBUG_SINK, "[%s:%d] ", __FILE_NAME__, __LINE__);              \
        fprintf(DEBUG_SINK, __VA_ARGS__);                                      \
        fflush(DEBUG_SINK);                                                    \
    } while (0)

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
