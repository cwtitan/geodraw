// Linux links: X11 GL
// Windows links: user32.lib Ws2_32.lib gdi32.lib OpenGL32.lib

#ifndef __TPKAPI__
#define __TPKAPI__

// Establish __windows__ symbol
#if !(defined __windows__) && (defined _WIN32 || defined _WIN64)
#define __windows__
#endif

// Some scary thing Microsoft does
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Linux includes
#ifdef __linux__
#include <pthread.h>
#include <GL/gl.h>
#include <sys/socket.h>
#endif

// Windows includes
#ifdef __windows__
#include <windows.h>
#include <winsock2.h>
#include <GL/gl.h>
#define strcasecomp _stricmp
#endif

// Error constants
#define TPK_ERR_UNKNOWN  -1
#define TPK_ERR_NONE      0
#define TPK_ERR_PREBOUND  2

// Boolean constants
#define TPK_FALSE 0
#define TPK_TRUE  1

// Event constants
#define TPK_EVENT_NONE      0
#define TPK_EVENT_CLOSE     1
#define TPK_EVENT_MOVE      2
#define TPK_EVENT_RESIZE    3
#define TPK_EVENT_KEYDOWN   4
#define TPK_EVENT_KEYUP     5
#define TPK_EVENT_MOUSEDOWN 6
#define TPK_EVENT_MOUSEUP   7
#define TPK_EVENT_MOUSEMOVE 8

// Mouse button constants
#define TPK_MOUSE_LEFT       1
#define TPK_MOUSE_RIGHT      2
#define TPK_MOUSE_MIDDLE     3
#define TPK_MOUSE_SCROLLUP   4
#define TPK_MOUSE_SCROLLDOWN 5

// Container for window information
typedef struct {
    char  text[256]; // The text displayed in the window's title bar
    int   width;     // The width of the window's client area, in pixels
    int   height;    // The height of the window's client area, in pixels
    int   x;         // The left coordinate of the window, in pixels
    int   y;         // The top coordinate of the window, in pixels
    int   state;     // The current window state
    int   border;    // The window's current border type
    int   visible;   // Determines whether or not the window is visible
    void *rc;        // The rendering context (if any) bound to this window
} TPK_WINDOW;

// Container for OpenGL rendering context information
typedef struct {
    void *window; // The window this rendering context is bound to
} TPK_GLRC;

// "Containers" for other objects
#define TPK_THREAD void
#define TPK_MUTEX  void

// Function prototypes
int          tpkCaseComp(char *, char *);
TPK_GLRC*    tpkCreateGLRC(TPK_WINDOW *);
TPK_MUTEX*   tpkCreateMutex();
TPK_THREAD*  tpkCreateThread(void *, void *);
TPK_WINDOW*  tpkCreateWindow(int, int, char *);
void         tpkDelete(void *);
void         tpkDoEvents();
void         tpkExitThread(int);
void         tpkLockMutex(TPK_MUTEX *);
void         tpkMakeCurrent(TPK_GLRC *);
int          tpkNextEvent(void *, int *, int *);
void         tpkSleep(int);
int          tpkShutdown();
int          tpkStartup();
void         tpkSwapBuffers(TPK_GLRC *);
unsigned int tpkTimer(unsigned int *);
void         tpkUnlockMutex(TPK_MUTEX *);
void         tpkUpdate(void *);
int          tpkWaitForThread(TPK_THREAD *);

#endif // __TPKAPI__

