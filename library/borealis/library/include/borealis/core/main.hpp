#pragma once

typedef int (*main_func)(int argc, char *argv[]);

int WinRTRunApp(main_func mainFunction);

int SDL_main(int argc, char* argv[]);

typedef struct HINSTANCE__ *HINSTANCE;
typedef char* PSTR, *LPSTR;
typedef wchar_t* PWSTR, *LPWSTR;
#ifdef main
#undef main
#endif
/* The VC++ compiler needs main/wmain defined, but not for GDK */
#if defined(_MSC_VER) && !defined(__GDK__)
#if defined(UNICODE) && UNICODE
int wmain(int argc, wchar_t *wargv[], wchar_t *wenvp)
{
    (void) argc;
    (void) wargv;
    (void) wenvp;
    return WinRTRunApp(SDL_main);
}
#else  /* ANSI */
int main(int argc, char *argv[])
{
    (void) argc;
    (void) argv;
    return WinRTRunApp(SDL_main);
}
#endif /* UNICODE */
#endif /* _MSC_VER && ! __GDK__ */

#if defined(UNICODE) && UNICODE
int __stdcall wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR szCmdLine, int sw)
#else /* ANSI */
int __stdcall WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
#endif
{
    (void) hInst;
    (void) hPrev;
    (void) szCmdLine;
    (void) sw;
    return WinRTRunApp(SDL_main);
}
