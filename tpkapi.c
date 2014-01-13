// Common includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tpkapi.h"

// Type constants for data objects
#define TPK_TYPE_GLRC   1
#define TPK_TYPE_WINDOW 2
#define TPK_TYPE_THREAD 3
#define TPK_TYPE_MUTEX  4

// Include Linux implementations
#ifdef __linux__
#include <unistd.h>
#include <X11/Xlib.h>
#include <GL/glx.h> 
#include <strings.h>
#include "tpkapi_linux.c"
#endif

// Include Windows implementations
#ifdef __windows__
#include <ws2tcpip.h>
#include "tpkapi_windows.c"
#endif



////////////////////////////////////////////////////////////////////////////////
//                              Common Functions                              //
////////////////////////////////////////////////////////////////////////////////

// Compares two strings while ignoring case
int tpkCaseComp(char *s1, char *s2) {
    #ifdef __linux__
        return strcasecmp(s1, s2);
    #endif
    #ifdef __windows__
        return _stricmp(s1, s2);
    #endif
}

// Deletes an object depending on its type
void tpkDelete(void *objptr) {
    int type;

    // Error checking
    if (!API_ACTIVE || objptr == NULL) return;

    // Get the pointer type field;
    objptr = (void *) (((char *) objptr) - sizeof (int));
    type = *((int *) objptr);

    // Process by object type
    switch (type) {
        case TPK_TYPE_GLRC:   deleteGLRC(objptr);   break;
        case TPK_TYPE_MUTEX:  deleteMutex(objptr);  break;
        case TPK_TYPE_THREAD: deleteThread(objptr); break;
        case TPK_TYPE_WINDOW: deleteWindow(objptr); break;
        default: break;
    }

    return;
}

// Select the current window and rendering context for the OpenGL pipeline
void tpkMakeCurrent(TPK_GLRC *rc) {
    TPK_WINDOW_EXT *wnd;
    TPK_GLRC_EXT   *xrc;

    // Error checking
    if (!API_ACTIVE || rc == NULL) return;

    // Resolve the window and GLRC references
    xrc = (TPK_GLRC_EXT *)   (((char *) rc) - sizeof (int));
    wnd = (TPK_WINDOW_EXT *) (((char *) xrc->self.window) - sizeof (int));

    // Select the window and rendering context
    #ifdef __linux__
        glXMakeCurrent(hDpy, wnd->hwnd, xrc->rc);
    #endif
    #ifdef __windows__
        wglMakeCurrent(wnd->hdc, xrc->rc);
    #endif

    // Update the current window and rendering context and exit
    wCur = wnd;
    gCur = xrc;
    return;
}

// Retrieves the next event for a given object
int tpkNextEvent(void *objptr, int *arg1, int *arg2) {
    int type;

    // Error checking
    if (!API_ACTIVE) return TPK_EVENT_NONE;

    // Get the pointer type field;
    if (objptr == NULL) return TPK_EVENT_NONE;
    objptr = (void *) (((char *) objptr) - sizeof (int));
    type = *((int *) objptr);

    // Process by object type
    switch (type) {
    case TPK_TYPE_WINDOW:
        return nextEventWindow(objptr, arg1, arg2); break;
    default: break;
    }

    // Default return value
    *arg1 = *arg2 = 0;
    return TPK_EVENT_NONE;
}

// Issue a SwapBuffers command
void tpkSwapBuffers(TPK_GLRC *rc) {
    TPK_GLRC_EXT *xrc;
    TPK_WINDOW_EXT *wnd;

    // Error checking
    if (!API_ACTIVE || rc == NULL) return;
    xrc = (TPK_GLRC_EXT *) (((char *) rc) - sizeof (int));
    if (xrc->self.window == NULL) return;
    wnd = (TPK_WINDOW_EXT *) (((char *) xrc->self.window) - sizeof (int));

    // Issue the command
    #ifdef __linux__
        glXSwapBuffers(hDpy, wnd->hwnd);
    #endif
    #ifdef __windows__
        SwapBuffers(wnd->hdc);
    #endif

    return;
}

// Updates an object's properties depending on type
void tpkUpdate(void *objptr) {
    int type;

    // Error checking
    if (!API_ACTIVE) return;

    // Get the pointer type field;
    if (objptr == NULL) return;
    objptr = (void *) (((char *) objptr) - sizeof (int));
    type = *((int *) objptr);

    // Process by object type
    switch (type) {
        case TPK_TYPE_WINDOW: updateWindow(objptr); break;
        default: break;
    }

    return;
}
