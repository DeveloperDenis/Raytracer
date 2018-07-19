#if !defined(PLATFORM_LAYER_H_)
#define PLATFORM_LAYER_H_

#include "denis_types.h"
#include "denis_math.h"

//TODO(denis): this doesn't take into account that the pitch may be different from the width of
// the image
//TODO(denis): maybe this should be defined in denis_drawing.h?
struct Bitmap
{
    u32* pixels;

    u32 width;
    u32 height;
};

struct Controller
{
    bool upPressed;
    bool downPressed;
    bool leftPressed;
    bool rightPressed;

    bool actionPressed;
};

struct Mouse
{
    v2 pos;
	
    bool leftPressed;
    bool leftWasPressed;
    v2 leftClickStartPos;

    bool rightPressed;
    bool rightWasPressed;
    v2 rightClickStartPos;
};

struct Touch
{
    u32 numActivePoints;
    v2 points[5];
};

struct Pen
{
    u32 pressure;
    u32 x;
    u32 y;
    bool usingEraser;
};

struct Input
{
    Pen pen;
    Touch touch;
    Mouse mouse;
    Controller controller;
};

void createThread(void(*threadFunction)(void*), void* data);
typedef void(*CreateThreadPtr)(void(*)(void*), void* data);

void acquireLock(volatile u32* lock);
typedef void(*AcquireLockPtr)(volatile u32*);

void releaseLock(volatile u32* lock);
typedef void(*ReleaseLockPtr)(volatile u32*);

struct PlatformFunctions
{
	CreateThreadPtr createThread;
	AcquireLockPtr acquireLock;
	ReleaseLockPtr releaseLock;
};

#define APP_INIT_CALL(name) void (name)(Memory* memory, PlatformFunctions platformFunctions)
#define MAIN_UPDATE_CALL(name) void (name)(Memory* memory, Bitmap* screen, Input* input, PlatformFunctions platformFunctions)

#if defined(DENIS_WIN32)

#define HEAP_ALLOC(size) VirtualAlloc(0, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE)
#define HEAP_FREE(pointer) VirtualFree(pointer, 0, MEM_RELEASE);
#include "win32_layer.cpp"

#elif defined(DENIS_LINUX)

#define HEAP_ALLOC(size) malloc(size)
#define HEAP_FREE(pointer) free(pointer)
#include "linux_layer.cpp"

#endif

#endif
