#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>

#include "cliopts.h"
#include "fs/fs.h"
#include "configfile.h"
#include "platform.h"

/* NULL terminated list of platform specific read-only data paths */
/* priority is top first */
const char *sys_ropaths[] = {
    ".", // working directory
    "!", // executable directory
#if defined(__linux__) || defined(__unix__)
    // some common UNIX directories for read only stuff
    "/usr/local/share/sm64practice",
    "/usr/share/sm64practice",
    "/opt/sm64practice",
#endif
    NULL,
};

/* these are not available on some platforms, so might as well */

char *sys_strlwr(char *src) {
  for (unsigned char *p = (unsigned char *)src; *p; p++)
     *p = tolower(*p);
  return src;
}

char *sys_strdup(const char *src) {
    const unsigned len = strlen(src) + 1;
    char *newstr = malloc(len);
    if (newstr) memcpy(newstr, src, len);
    return newstr;
}

int sys_strcasecmp(const char *s1, const char *s2) {
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    int result;
    if (p1 == p2)
        return 0;
    while ((result = tolower(*p1) - tolower(*p2++)) == 0)
        if (*p1++ == '\0')
            break;
    return result;
}

const char *sys_file_extension(const char *fpath) {
    const char *fname = sys_file_name(fpath);
    const char *dot = strrchr(fname, '.');
    if (!dot || !dot[1]) return NULL; // no dot
    if (dot == fname) return NULL; // dot is the first char (e.g. .local)
    return dot + 1;
}

const char *sys_file_name(const char *fpath) {
    const char *sep1 = strrchr(fpath, '/');
    const char *sep2 = strrchr(fpath, '\\');
    const char *sep = sep1 > sep2 ? sep1 : sep2;
    if (!sep) return fpath;
    return sep + 1;
}

#define ONE_BILLION 1000000000ULL

static inline uint64_t get_monotonic_time(void){
	struct timespec ts;
	
	clock_gettime(CLOCK_MONOTONIC,&ts);
	return ts.tv_sec*ONE_BILLION+ts.tv_nsec;
}

void sys_sleep(const uint64_t us) {
    // TODO: figure out which of the platforms we want to support DOESN'T have usleep()
    //usleep(us);
	uint64_t ns = us*1000;
	uint64_t start = get_monotonic_time();
	uint64_t diff;
	while (1){
		diff = get_monotonic_time()-start;
		if (diff>ns)
			break;
	}
}

/* this calls a platform-specific impl function after forming the error message */

static void sys_fatal_impl(const char *msg) __attribute__ ((noreturn));

void sys_fatal(const char *fmt, ...) {
    static char msg[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    fflush(stdout);
    sys_fatal_impl(msg);
}

#ifdef HAVE_SDL2
#include <SDL2/SDL.h>

const char *sys_user_path(void) {
    static char path[SYS_MAX_PATH] = { 0 };

    // get the new pref path from SDL
    char *sdlpath = SDL_GetPrefPath("", "sm64practice");
    if (sdlpath) {
        const unsigned int len = strlen(sdlpath);
        strncpy(path, sdlpath, sizeof(path));
        path[sizeof(path)-1] = 0;

        SDL_free(sdlpath);

        if (path[len-1] == '/' || path[len-1] == '\\')
            path[len-1] = 0; // strip the trailing separator

        if (!fs_sys_dir_exists(path) && !fs_sys_mkdir(path))
            path[0] = 0; // somehow failed, we got no user path
    }

    return path;
}

const char *sys_exe_path(void) {
    static char path[SYS_MAX_PATH] = { 0 };
    char *sdlpath = SDL_GetBasePath();
    if (sdlpath && sdlpath[0]) {
        // use the SDL path if it exists
        const unsigned int len = strlen(sdlpath);
        strncpy(path, sdlpath, sizeof(path));
        path[sizeof(path)-1] = 0;
        SDL_free(sdlpath);
        if (path[len-1] == '/' || path[len-1] == '\\')
            path[len-1] = 0; // strip the trailing separator
    }
    return path;
}

static void sys_fatal_impl(const char *msg) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR , "Fatal error", msg, NULL);
    fprintf(stderr, "FATAL ERROR:\n%s\n", msg);
    fflush(stderr);
    exit(1);
}

#else

#warning "You might want to implement these functions for your platform"

const char *sys_user_path(void) {
    return ".";
}

const char *sys_exe_path(void) {
    return ".";
}

static void sys_fatal_impl(const char *msg) {
    fprintf(stderr, "FATAL ERROR:\n%s\n", msg);
    fflush(stderr);
    exit(1);
}

#endif // platform switch
