#ifdef __WINRT__

#include <SDL_main.h>
#include <SDL_syswm.h>

typedef int (*main_func)(int argc, char *argv[]);

int WinRTRunApp(main_func mainFunction) {
    return SDL_WinRTRunApp(mainFunction, NULL);
}

#endif