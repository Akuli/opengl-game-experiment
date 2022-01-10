#ifndef LOG_H
#define LOG_H

#include <SDL2/SDL.h>      // IWYU pragma: keep

void log_init(void);

// https://stackoverflow.com/a/5459929
#define LOG_STR_HELPER(x) #x
#define LOG_STR(x) LOG_STR_HELPER(x)

#define log_printf(...) SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, __FILE__ ":" LOG_STR(__LINE__) ": " __VA_ARGS__)
#define log_printf_abort(...) (log_printf(__VA_ARGS__), abort())

#endif   // LOG_H
