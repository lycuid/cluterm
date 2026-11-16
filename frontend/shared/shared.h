#define die(code, ...)                                                         \
    do {                                                                       \
        printf(__VA_ARGS__);                                                   \
        exit(code);                                                            \
    } while (0)
