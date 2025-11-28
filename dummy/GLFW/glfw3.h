#ifndef _glfw3_h_
#define _glfw3_h_

#include <stddef.h>
#include <stdint.h>

#define GLFW_TRUE 1
#define GLFW_FALSE 0
#define GLFW_RELEASE 0
#define GLFW_PRESS 1
#define GLFW_REPEAT 2

#define GLFW_KEY_LAST 348
#define GLFW_MOUSE_BUTTON_LAST 7
#define GLFW_CURSOR 0x00033001
#define GLFW_CURSOR_NORMAL 0x00034001
#define GLFW_CURSOR_HIDDEN 0x00034002
#define GLFW_CURSOR_DISABLED 0x00034003

#define GLFW_CONNECTED 0x00040001
#define GLFW_DISCONNECTED 0x00040002

#define GLFW_ARROW_CURSOR 0x00036001
#define GLFW_IBEAM_CURSOR 0x00036002
#define GLFW_CROSSHAIR_CURSOR 0x00036003
#define GLFW_HAND_CURSOR 0x00036004
#define GLFW_HRESIZE_CURSOR 0x00036005
#define GLFW_VRESIZE_CURSOR 0x00036006

#define GLFW_FOCUSED 0x00020001
#define GLFW_ICONIFIED 0x00020002
#define GLFW_MAXIMIZED 0x00020005
#define GLFW_VISIBLE 0x00020004
#define GLFW_DECORATED 0x00020005
#define GLFW_FLOATING 0x00020007
#define GLFW_RESIZABLE 0x00020003
#define GLFW_SAMPLES 0x0002100D
#define GLFW_CONTEXT_VERSION_MAJOR 0x00022002
#define GLFW_CONTEXT_VERSION_MINOR 0x00022003
#define GLFW_OPENGL_PROFILE 0x00022008
#define GLFW_OPENGL_CORE_PROFILE 0x00032001
#define GLFW_OPENGL_FORWARD_COMPAT 0x00022006
#define GLFW_CLIENT_API 0x00022001
#define GLFW_NO_API 0

#define GLFW_DONT_CARE -1

#define GLFW_GAMEPAD_AXIS_LEFT_X 0
#define GLFW_GAMEPAD_AXIS_LEFT_Y 1
#define GLFW_GAMEPAD_AXIS_RIGHT_X 2
#define GLFW_GAMEPAD_AXIS_RIGHT_Y 3

typedef struct GLFWwindow GLFWwindow;
typedef struct GLFWmonitor GLFWmonitor;
typedef struct GLFWcursor GLFWcursor;

typedef struct GLFWvidmode {
    int width;
    int height;
    int redBits;
    int greenBits;
    int blueBits;
    int refreshRate;
} GLFWvidmode;

typedef struct GLFWimage {
    int width;
    int height;
    unsigned char* pixels;
} GLFWimage;

typedef struct GLFWgamepadstate {
    unsigned char buttons[15];
    float axes[6];
} GLFWgamepadstate;

typedef void (*GLFWerrorfun)(int, const char*);
typedef void (*GLFWdropfun)(GLFWwindow*, int, const char**);
typedef void (*GLFWwindowfocusfun)(GLFWwindow*, int);
typedef void (*GLFWwindowiconifyfun)(GLFWwindow*, int);
typedef void (*GLFWframebuffersizefun)(GLFWwindow*, int, int);
typedef void (*GLFWkeyfun)(GLFWwindow*, int, int, int, int);
typedef void (*GLFWcharfun)(GLFWwindow*, unsigned int);
typedef void (*GLFWmousebuttonfun)(GLFWwindow*, int, int, int);
typedef void (*GLFWcursorposfun)(GLFWwindow*, double, double);
typedef void (*GLFWscrollfun)(GLFWwindow*, double, double);
typedef void (*GLFWjoystickfun)(int, int);

// Functions
int glfwInit(void);
void glfwTerminate(void);
void glfwSetErrorCallback(GLFWerrorfun cbfun);
GLFWwindow* glfwCreateWindow(int width, int height, const char* title, GLFWmonitor* monitor, GLFWwindow* share);
void glfwDestroyWindow(GLFWwindow* window);
int glfwWindowShouldClose(GLFWwindow* window);
void glfwPollEvents(void);
void glfwSwapBuffers(GLFWwindow* window);
void glfwMakeContextCurrent(GLFWwindow* window);
void glfwWindowHint(int hint, int value);
void glfwGetWindowSize(GLFWwindow* window, int* width, int* height);
void glfwSetWindowSize(GLFWwindow* window, int width, int height);
void glfwSetWindowPos(GLFWwindow* window, int xpos, int ypos);
void glfwGetWindowPos(GLFWwindow* window, int* xpos, int* ypos);
int glfwGetWindowAttrib(GLFWwindow* window, int attrib);
void glfwSetWindowAttrib(GLFWwindow* window, int attrib, int value);
void glfwSetWindowTitle(GLFWwindow* window, const char* title);
void glfwSetWindowIcon(GLFWwindow* window, int count, const GLFWimage* images);
GLFWmonitor* glfwGetWindowMonitor(GLFWwindow* window);
void glfwSetWindowMonitor(GLFWwindow* window, GLFWmonitor* monitor, int xpos, int ypos, int width, int height, int refreshRate);
GLFWmonitor* glfwGetPrimaryMonitor();
GLFWmonitor** glfwGetMonitors(int* count);
const GLFWvidmode* glfwGetVideoMode(GLFWmonitor* monitor);
const GLFWvidmode* glfwGetVideoModes(GLFWmonitor* monitor, int* count);
void glfwGetMonitorPos(GLFWmonitor* monitor, int* xpos, int* ypos);
void glfwGetMonitorPhysicalSize(GLFWmonitor* monitor, int* widthMM, int* heightMM);
const char* glfwGetMonitorName(GLFWmonitor* monitor);
void glfwSetWindowOpacity(GLFWwindow* window, float opacity);
void glfwIconifyWindow(GLFWwindow* window);
void glfwRestoreWindow(GLFWwindow* window);
void glfwMaximizeWindow(GLFWwindow* window);
void glfwShowWindow(GLFWwindow* window);
void glfwHideWindow(GLFWwindow* window);
void glfwFocusWindow(GLFWwindow* window);
void glfwSetInputMode(GLFWwindow* window, int mode, int value);
GLFWcursor* glfwCreateStandardCursor(int shape);
void glfwDestroyCursor(GLFWcursor* cursor);
void glfwSetCursor(GLFWwindow* window, GLFWcursor* cursor);
void glfwSetCursorPos(GLFWwindow* window, double xpos, double ypos);
void glfwGetCursorPos(GLFWwindow* window, double* xpos, double* ypos);
const char* glfwGetClipboardString(GLFWwindow* window);
void glfwSetClipboardString(GLFWwindow* window, const char* string);
double glfwGetTime(void);
void glfwSwapInterval(int interval);
int glfwJoystickPresent(int jid);
int glfwJoystickIsGamepad(int jid);
const char* glfwGetJoystickName(int jid);
const float* glfwGetJoystickAxes(int jid, int* count);
int glfwGetGamepadState(int jid, GLFWgamepadstate* state);
int glfwUpdateGamepadMappings(const char* string);
void* glfwGetProcAddress(const char* procname);
void glfwGetWindowContentScale(GLFWwindow* window, float* xscale, float* yscale);
void glfwGetFramebufferSize(GLFWwindow* window, int* width, int* height);
void glfwSetWindowSizeLimits(GLFWwindow* window, int minwidth, int minheight, int maxwidth, int maxheight);

// Callbacks setters
GLFWdropfun glfwSetDropCallback(GLFWwindow* window, GLFWdropfun cbfun);
GLFWwindowfocusfun glfwSetWindowFocusCallback(GLFWwindow* window, GLFWwindowfocusfun cbfun);
GLFWwindowiconifyfun glfwSetWindowIconifyCallback(GLFWwindow* window, GLFWwindowiconifyfun cbfun);
GLFWframebuffersizefun glfwSetFramebufferSizeCallback(GLFWwindow* window, GLFWframebuffersizefun cbfun);
GLFWkeyfun glfwSetKeyCallback(GLFWwindow* window, GLFWkeyfun cbfun);
GLFWcharfun glfwSetCharCallback(GLFWwindow* window, GLFWcharfun cbfun);
GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow* window, GLFWmousebuttonfun cbfun);
GLFWcursorposfun glfwSetCursorPosCallback(GLFWwindow* window, GLFWcursorposfun cbfun);
GLFWscrollfun glfwSetScrollCallback(GLFWwindow* window, GLFWscrollfun cbfun);
GLFWjoystickfun glfwSetJoystickCallback(GLFWjoystickfun cbfun);

#endif
