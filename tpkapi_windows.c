// Includes are processed in tpkapi.c

// Doesn't seem to always be defined for whatever reason
#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif

// Additional constants not seen to the public API
#define TPK_EVENT_UNKNOWN -1

// Internal extended data structure for window information
typedef struct {
    int  type;          // Object type field
    TPK_WINDOW user;    // User-visible data structure
    TPK_WINDOW self;    // Internal data structure
    char classname[32]; // String data used for class name
    HINSTANCE hinst;    // OS-specific instance handle
    HWND hwnd;          // OS-specific window handle
    HDC  hdc;           // OS-specific device context handle
    int  hasMove;       // Move events aren't added to queue
    int  hasResize;     // Resize events aren't added to queue
} TPK_WINDOW_EXT;

// Internal extended data structure for OpenGL rendering context information
typedef struct {
    int type;        // Object type field
    TPK_GLRC user;   // User-visible data structure
    TPK_GLRC self;   // Internal data structure
    HGLRC rc;        // OS-specific rendering context handle
} TPK_GLRC_EXT;

// Internal extended data structure for thread information
typedef struct {
    int type;
    HANDLE hThread;
} TPK_THREAD_EX;

// Internal extended data structure for mutex information
typedef struct {
    int type;
    CRITICAL_SECTION hMutex;
} TPK_MUTEX_EX;

// Private variables for most recent window event
#define WndEvent(x, y, z) wEvent = x; wArg1 = y; wArg2 = z; return 0
int wEvent, wArg1, wArg2, API_ACTIVE = TPK_FALSE;
TPK_GLRC_EXT   *gCur;
TPK_WINDOW_EXT *wCur;



////////////////////////////////////////////////////////////////////////////////
//                              Window Functions                              //
////////////////////////////////////////////////////////////////////////////////

// Window callback function used by Windows.
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, 
    LPARAM lParam){
    TPK_WINDOW_EXT *wnd = 
        (TPK_WINDOW_EXT *) GetWindowLongPtr(hWnd, GWL_USERDATA);

    // If window is still being constructed, there's no user data yet
    if (!wnd)
	return DefWindowProc(hWnd, uMsg, wParam, lParam);

    // Determine what events to handle
    switch (uMsg) {

    // Window close request
    case WM_CLOSE: WndEvent(TPK_EVENT_CLOSE, 0, 0);

    // Key Down and Key Up
    case WM_KEYDOWN: WndEvent(TPK_EVENT_KEYDOWN, wParam, 0);
    case WM_KEYUP:   WndEvent(TPK_EVENT_KEYUP,   wParam, 0);

    // Mouse Down
    case WM_LBUTTONDOWN: WndEvent(TPK_EVENT_MOUSEDOWN, TPK_MOUSE_LEFT,   0);
    case WM_RBUTTONDOWN: WndEvent(TPK_EVENT_MOUSEDOWN, TPK_MOUSE_RIGHT,  0);
    case WM_MBUTTONDOWN: WndEvent(TPK_EVENT_MOUSEDOWN, TPK_MOUSE_MIDDLE, 0);

    // Mouse scroll-wheel interprated as MouseDown 4 and 5
    case WM_MOUSEWHEEL:
    if (wParam >> 31) {
        WndEvent(TPK_EVENT_MOUSEDOWN, TPK_MOUSE_SCROLLDOWN, 0);
    } else {
        WndEvent(TPK_EVENT_MOUSEDOWN, TPK_MOUSE_SCROLLUP,   0);
    }

    // Mouse Move
    case WM_MOUSEMOVE:
        WndEvent(TPK_EVENT_MOUSEMOVE, LOWORD(lParam), HIWORD(lParam));

    // Mouse Up
    case WM_LBUTTONUP: WndEvent(TPK_EVENT_MOUSEUP, TPK_MOUSE_LEFT,   0);
    case WM_RBUTTONUP: WndEvent(TPK_EVENT_MOUSEUP, TPK_MOUSE_RIGHT,  0);
    case WM_MBUTTONUP: WndEvent(TPK_EVENT_MOUSEUP, TPK_MOUSE_MIDDLE, 0);

    // Move
    case WM_MOVE:
        wnd->hasMove = TPK_TRUE; return 0;

    // Resize
    case WM_SIZE:
        wnd->hasResize = TPK_TRUE; return 0;

    default: break;
    } // switch

    // All other events processed by Windows
    wEvent = TPK_EVENT_UNKNOWN;
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// Retrieves the location and dimensions of a window's client area
static void measureWindow(TPK_WINDOW_EXT *wnd) {
    RECT r;

    // Retrieve the information from the system and apply to the window object
    GetWindowRect(wnd->hwnd, &r);
    wnd->user.x = wnd->self.x = r.left;
    wnd->user.y = wnd->self.y = r.top;
    GetClientRect(wnd->hwnd, &r);
    wnd->user.width  = wnd->self.width  = r.right  - r.left;
    wnd->user.height = wnd->self.height = r.bottom - r.top;

    return;
}

// Creates a window
TPK_WINDOW* tpkCreateWindow(int width, int height, char *text) {
    WNDCLASS C;
    TPK_WINDOW_EXT *wnd;

    // Error checking
    if (!API_ACTIVE) return NULL;

    // Initialize variables
    wnd = malloc(sizeof (TPK_WINDOW_EXT));
    wnd->type = TPK_TYPE_WINDOW;
    wnd->user.rc = wnd->self.rc = NULL;
    wnd->user.text[0] = 0;
    strncat(wnd->user.text, text, 255);
    strcpy(wnd->self.text, wnd->user.text);
    wnd->self.x = wnd->self.y = wnd->self.width = wnd->self.height = -1;
    wnd->self.visible = wnd->hasMove = wnd->hasResize = TPK_FALSE;

    // Initialize user component
    width  = (width  < 0) ? 0 : width;
    height = (height < 0) ? 0 : height;
    wnd->user.x = 0;
    wnd->user.y = 0;
    wnd->user.width = width;
    wnd->user.height = height;
    wnd->user.visible = TPK_TRUE;

    // Prepare some Windows-specific values
    sprintf(wnd->classname, "%x", (unsigned int) wnd);
    wnd->hinst = GetModuleHandle(NULL);

    // Window class structure to register with the system
    C.style         = CS_OWNDC;
    C.lpfnWndProc   = (WNDPROC) WindowProc;
    C.cbClsExtra    = 0;
    C.cbWndExtra    = 0;
    C.hInstance     = wnd->hinst;
    C.hIcon         = LoadIcon(NULL, IDI_WINLOGO);
    C.hCursor       = LoadCursor(NULL, IDC_ARROW);
    C.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
    C.lpszMenuName  = NULL;
    C.lpszClassName = wnd->classname;

    // Attempt to register the class
    wnd->hwnd = NULL;
    if (!RegisterClass(&C)) {
        free(wnd);
        return NULL;
    }

    // Attempt to create the window
    wnd->hwnd = CreateWindow(wnd->classname, (text == NULL) ? "" : text, 
        WS_POPUPWINDOW, 32, 32, width, height, NULL, NULL, wnd->hinst, NULL);
    if (wnd->hwnd == NULL) {
        UnregisterClass(wnd->classname, wnd->hinst);
        free(wnd);
        return NULL;
    }

    // Configure the remainder of the window
    SetWindowLongPtr(wnd->hwnd, GWL_USERDATA, (LONG_PTR) wnd);
    wnd->hdc = GetDC(wnd->hwnd);
    SetWindowLong(wnd->hwnd, GWL_STYLE,   0x06CF0000);
    SetWindowLong(wnd->hwnd, GWL_EXSTYLE, 0x00040100);
    tpkUpdate(&wnd->user);
    measureWindow(wnd);

    // Return a pointer to only the user-visible portion of the data structure
    return &wnd->user;
}

// Processes the next window event
static int nextEventWindow(TPK_WINDOW_EXT *wnd, int *arg1, int *arg2) {
    MSG m;
    int ret;

    // Catch Microsoft's move event mistakes
    if (wnd->hasMove) {
        wnd->hasMove = TPK_FALSE;
        measureWindow(wnd);
        *arg1 = wnd->self.x;
        *arg2 = wnd->self.y;
        return TPK_EVENT_MOVE;
    }

    // Catch Microsoft's resize event mistakes
    if (wnd->hasResize) {
        wnd->hasResize = TPK_FALSE;
        measureWindow(wnd);
        *arg1 = wnd->self.width;
        *arg2 = wnd->self.height;
        return TPK_EVENT_RESIZE;
    }

    // Skip events until a known event is encountered
    else {
        for (wEvent = TPK_EVENT_UNKNOWN; wEvent == TPK_EVENT_UNKNOWN; ) {
            if (PeekMessage(&m, wnd->hwnd, 0, 0, PM_REMOVE)) {
                TranslateMessage(&m);
                DispatchMessage(&m);
            } else wEvent = TPK_EVENT_NONE;
        }
    }

    // Set parameters and exit
    *arg1 = wArg1;
    *arg2 = wArg2;
    ret = wEvent;
    wEvent = TPK_EVENT_NONE;
    return ret;
}

// Updates a window's properties
static void updateWindow(TPK_WINDOW_EXT *wnd) {
    RECT r;

    // Visibility
    if (wnd->user.visible != wnd->self.visible) {
        wnd->self.visible = (wnd->user.visible) ? TPK_TRUE : TPK_FALSE;
        wnd->user.visible = wnd->self.visible;
        ShowWindow(wnd->hwnd, (wnd->self.visible) ? SW_SHOW : SW_HIDE);
    }

    // Window Title
    if (strcmp(wnd->user.text, wnd->self.text)) {
        wnd->self.text[0] = 0;
        strncat(wnd->self.text, wnd->user.text, 255);
        strcpy(wnd->user.text, wnd->self.text);
        SetWindowText(wnd->hwnd, wnd->self.text);
    }

    // Resize/position the window
    if (wnd->user.x      != wnd->self.x     || 
        wnd->user.y      != wnd->self.y     ||
        wnd->user.width  != wnd->self.width ||
        wnd->user.height != wnd->self.height) {

        // Move and size the window by system function
        r.left  = 0;               r.top    = 0;
        r.right = wnd->user.width; r.bottom = wnd->user.height;
        AdjustWindowRect(&r, 0x06CF0000, 0);
        MoveWindow(wnd->hwnd, wnd->user.x, wnd->user.y,
            r.right - r.left, r.bottom - r.top, 1);
        measureWindow(wnd);
    }

    return;
}

// Deletes a window
static void deleteWindow(TPK_WINDOW_EXT *wnd) {

    // Deselect the window from OpenGL, if applicable
    if (wnd == wCur) {
        wglMakeCurrent(NULL, NULL);
        wCur = NULL; gCur = NULL;
    }

    // Delete the OpenGL rendering context, if applicable
    if (wnd->self.rc != NULL)
        tpkDelete(wnd->self.rc);

    // Delete the window
    ReleaseDC(wnd->hwnd, wnd->hdc);
    DestroyWindow(wnd->hwnd);
    UnregisterClass(wnd->classname, wnd->hinst);

    // Deallocate memory and return
    free(wnd);
    return;
}



////////////////////////////////////////////////////////////////////////////////
//                              OpenGL Functions                              //
////////////////////////////////////////////////////////////////////////////////

// Create an OpenGL rendering context
TPK_GLRC* tpkCreateGLRC(TPK_WINDOW *wnd) {
    PIXELFORMATDESCRIPTOR pfd;
    TPK_WINDOW_EXT *xwnd;
    TPK_GLRC_EXT *rc;
    int pf;

    // Error checking
    if (!API_ACTIVE || wnd == NULL) return NULL;
    xwnd = (TPK_WINDOW_EXT *) (((char *) wnd) - sizeof (int));
    if (xwnd->self.rc != NULL) return NULL; // Can't bind two contexts

    // Attempt to allocate the needed memory
    rc = malloc(sizeof(TPK_GLRC_EXT));
    if (rc == NULL) return NULL;
    rc->type = TPK_TYPE_GLRC;
    rc->user.window  = rc->self.window  = (void *) wnd;

    // Configure needed PixelFormatDescriptor elements
    memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize      = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 16;
    pfd.iLayerType = PFD_MAIN_PLANE;

    // Attempt to create the rendering context
    pf = ChoosePixelFormat(xwnd->hdc, &pfd);
    SetPixelFormat(xwnd->hdc, pf, &pfd);
    rc->rc = wglCreateContext(xwnd->hdc);
    if (rc->rc == NULL) {
        free(rc);
        return NULL;
    }

    // Return the handle to the object
    xwnd->user.rc = xwnd->self.rc = &rc->user;
    return &rc->user;
}

// Deletes an OpenGL rednering context
static void deleteGLRC(TPK_GLRC_EXT *rc) {

    // Deselect the rendering context from OpenGL, if applicable
    if (rc == gCur) {
        wglMakeCurrent(NULL, NULL);
        wCur->user.rc = wCur->self.rc = NULL;
        wCur = NULL; gCur = NULL;
    }

    // Delete the rendering context
    wglDeleteContext(rc->rc);

    // Deallocate memory and return
    free(rc);
    return;
}



////////////////////////////////////////////////////////////////////////////////
//                          Multithreading Functions                          //
////////////////////////////////////////////////////////////////////////////////

// Creates a new mutex
TPK_MUTEX* tpkCreateMutex() {
    TPK_MUTEX_EX *xMutex = malloc(sizeof(TPK_MUTEX_EX));

    // Error checking
    if (!API_ACTIVE) return NULL;

    // Initialize the mutex object
    xMutex->type = TPK_TYPE_MUTEX;
    InitializeCriticalSection(&xMutex->hMutex);

    // Return the public handle
    return (TPK_MUTEX *) &xMutex->hMutex;
}

// Creates a new execution thread -- Executes immediately
TPK_THREAD* tpkCreateThread(void *entry, void *param) {
    HANDLE hThread;
    TPK_THREAD_EX *xThread;

    // Error checking
    if (!API_ACTIVE) return NULL;
    if (entry == NULL) return NULL;

    // Attempt to create a Windows thread
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) entry, 
        (LPVOID) param, 0, NULL);
    if (hThread == NULL) return NULL;

    // Construct a TPK thread object
    xThread = malloc(sizeof(TPK_THREAD_EX));
    xThread->type = TPK_TYPE_THREAD;
    xThread->hThread = hThread;

    // Return the public handle
    return (TPK_THREAD *) &xThread->hThread;
}

// Exits the current thread
void tpkExitThread(int exitcode) {
    ExitThread(exitcode);
    return; // Unreachable, but some compilers throw warnings without it
}

// Requests ownership of a mutex
void tpkLockMutex(TPK_MUTEX *mutex) {
    TPK_MUTEX_EX *xMutex;

    // Error checking
    if (mutex == NULL) return;

    // Request ownership of the mutex
    xMutex = (TPK_MUTEX_EX *) ((char *) mutex - sizeof(int));
    EnterCriticalSection(&xMutex->hMutex);
    return;
}

// Releases ownership of a mutex
void tpkUnlockMutex(TPK_MUTEX *mutex) {
    TPK_MUTEX_EX *xMutex;

    // Error checking
    if (mutex == NULL) return;

    // Request ownership of the mutex
    xMutex = (TPK_MUTEX_EX *) ((char *) mutex - sizeof(int));
    LeaveCriticalSection(&xMutex->hMutex);
    return;
}

// Waits for a thread to terminate
int tpkWaitForThread(TPK_THREAD *thread) {
    TPK_THREAD_EX *xThread;
    int ret;

    // Error checking
    if (thread == NULL) return 0;

    // Wait for the thread to exit
    xThread = (TPK_THREAD_EX *) ((char *) thread - sizeof(int));
    WaitForSingleObject(xThread->hThread, INFINITE);

    // Return the thread's exit code
    GetExitCodeThread(xThread->hThread, (LPDWORD) &ret);
    return ret;
}

// Deletes a mutex
static void deleteMutex(TPK_MUTEX_EX *xMutex) {
    free(xMutex);
    return;
}

// Deletes a thread
static void deleteThread(TPK_THREAD_EX *xThread) {
    free(xThread);
    return;
}



////////////////////////////////////////////////////////////////////////////////
//                             Abstract Functions                             //
////////////////////////////////////////////////////////////////////////////////

// Flushes out all remaining events
void tpkDoEvents() {
    MSG m;

    // Flush all events
    while (1) {
        if (PeekMessage(&m, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessage(&m);
        } else break;
    }

    return;
}

// Sleep for a given number of milliseconds
void tpkSleep(int ms) {
    SleepEx(ms, 0);
    return;
}

// Uninitialize the API
int tpkShutdown() {

    // Shut down WinSock
    WSACleanup();

    API_ACTIVE = TPK_FALSE;
    return TPK_ERR_NONE;
}

// Initialize the API
int tpkStartup() {
    WSADATA wsaData;

    // Error checking
    if (API_ACTIVE) return TPK_ERR_NONE;

    // Attempt to start up WinSock
    if (WSAStartup(MAKEWORD(2,2), &wsaData))
        return TPK_ERR_UNKNOWN;

    // Perform initialization routine
    wEvent = TPK_EVENT_NONE;
    API_ACTIVE = TPK_TRUE;
    wCur = NULL; gCur = NULL;
    return TPK_ERR_NONE;
}

// Return elapsed milliseconds between function calls
unsigned int tpkTimer(unsigned int *previous) {
    unsigned int change, thisms, lastms = *previous;
    LARGE_INTEGER a, b;

    // Get current time
    if (!QueryPerformanceCounter(&a)) {
        // Fall back to GetTickCount if necessary
        thisms = GetTickCount();

        // Calculate change in time
        if (thisms < lastms)
            change = (unsigned int) 0xFFFFFFFF - lastms + thisms;
        else
            change = thisms - lastms;

    } else {
        // Convert the value to milliseconds
        QueryPerformanceFrequency(&b);
        thisms = (unsigned int) (a.QuadPart * 1000 / b.QuadPart);
		change = thisms - lastms;
    }

    // Return ticks
    *previous = thisms;
    return change;
}
