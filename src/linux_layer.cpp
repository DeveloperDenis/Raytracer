#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysymdef.h>

#include <time.h>
#include <stdio.h>
#include <pthread.h>

#include "denis_types.h"
#include "platform_layer.h"
#include "denis_drawing.h"
#include "shared_platform_functions.h"

#define DEFAULT_WINDOW_WIDTH 640
#define DEFAULT_WINDOW_HEIGHT 480

struct Memory;

extern APP_INIT_CALL(appInit);

typedef MAIN_UPDATE_CALL(*mainUpdateCallPtr);
extern MAIN_UPDATE_CALL(mainUpdateCall);

struct LinuxThread
{
	void(*function)(void*);
	void* data;
};

static LinuxThread _linuxThread;

//NOTE(denis): provides a pass-through to translate between pthreads and our application threads
static void* thread(void* appThread)
{
	int oldType;
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, &oldType);
	
	LinuxThread* linuxThread = (LinuxThread*)appThread;
    linuxThread->function(linuxThread->data);

	return 0;
}

//TODO(denis): perhaps somewhat ironically, this function is NOT thread-safe. If we happen to call this twice simultaenously, both
// threads will not start properly since we only have one LinuxThread variable
// NOTE(denis): this isn't actually a problem for this application, since we only ever have two threads
// (main thread, and raytrace thread)
void createThread(void(*threadFunction)(void*), void* data)
{
	_linuxThread.function = threadFunction;
	_linuxThread.data = data;

	//TODO(denis): save the thread handle so that I can call pthread_cancel on the thread at the end of the main loop
	pthread_t threadHandle;
	pthread_create(&threadHandle, 0, thread, &_linuxThread);
}

static inline u32 xchg(volatile u32* value, u32 newValue)
{
	u32 result;
	
	__asm__ __volatile__("lock; xchgl %0, %1" :
						 "+m" (*value), "=a" (result) :
						 "1" (newValue) :
						 "cc");

	return result;
}

//TODO(denis): busy loops, not fantastic
inline void acquireLock(volatile u32* lock)
{
	while (xchg(lock, 1) == 1)
		;
}
inline void releaseLock(volatile u32* lock)
{
	while (xchg(lock, 0) == 0)
		;
}

//NOTE(denis): this only cares about nano second differences, and it assumes that the differences in nano seconds between two
// times will be small (less than 1 second)
static inline long getElapsedTimeNs(timespec* startTime, timespec* currTime)
{
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, currTime);
    long nanoDiff = currTime->tv_nsec - startTime->tv_nsec;
    if (nanoDiff < 0)
    {
		long diff1 = currTime->tv_nsec;
		long diff2 = 1000000000 - startTime->tv_nsec;
		nanoDiff = diff1 + diff2;
    }

    return nanoDiff;
}

static XImage* createXImage(Display* display, XVisualInfo* visualInfo, u32 width, u32 height, Bitmap* outBitmap)
{
    s32 pixelBits = 32;
    u32 pixelBytes = pixelBits/8;
    u32 backBufferSize = width*height*pixelBytes;
    u32* pixels = (u32*)malloc(backBufferSize);
    
    XImage* backBuffer = XCreateImage(display, visualInfo->visual, visualInfo->depth, ZPixmap, 0, (char*)pixels,
									  width, height, pixelBits, 0);

	outBitmap->pixels = pixels;
    outBitmap->width = width;
    outBitmap->height = height;

	return backBuffer;
}

int main(int argc, char** argv)
{
	u32 windowWidth = DEFAULT_WINDOW_WIDTH;
	u32 windowHeight = DEFAULT_WINDOW_HEIGHT;

	char* rayFileName = 0;
	
	for (s32 i = 1; i < argc; ++i)
	{
		bool argumentValid = false;
		
		if (windowWidth == DEFAULT_WINDOW_WIDTH && windowHeight == DEFAULT_WINDOW_HEIGHT)
		{
			if (getWindowSizeFromCommandLine(argv[1], &windowWidth, &windowHeight))
				argumentValid = true;
		}
		if (rayFileName == 0)
		{
			if (getRayFileFromCommandLine(argv[i], &rayFileName))
				argumentValid = true;
		}

		if (!argumentValid)
		{
			printf("Error in one or more command line arguments. Usage is: \"Raytracer 800x600 scene_file.ray\"\n");
			printf("Using default values.\n");
		}
	}

	if (rayFileName == 0)
		rayFileName = (char*)"default_scene.ray";
	
    Display* display = XOpenDisplay(0);
    u32 screenNum = DefaultScreen(display);
    int rootWindow = DefaultRootWindow(display);

    int screenDepth = 24;
    XVisualInfo visualInfo = {};
    if (!XMatchVisualInfo(display, screenNum, screenDepth, TrueColor, &visualInfo))
    {
		printf("Error matching visual info\n");
		exit(1);
    }

    XSetWindowAttributes windowAttributes = {};
    //TODO(denis): need to add more here as I add more event processing
    windowAttributes.event_mask =
		StructureNotifyMask|ButtonPressMask|KeyPressMask|KeyReleaseMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask;
    windowAttributes.colormap = XCreateColormap(display, rootWindow, visualInfo.visual, AllocNone);

    unsigned long attributeMask = CWBackPixel|CWColormap|CWEventMask;

    Window window = XCreateWindow(display, rootWindow, 0, 0, windowWidth, windowHeight, 0,
								  visualInfo.depth, InputOutput, visualInfo.visual, attributeMask, &windowAttributes);
    if (!window)
    {
		printf("Error creating window\n");
		exit(1);
    }
    
    XStoreName(display, window, "Raytracer");
    XMapRaised(display, window);

    XFlush(display);

    Bitmap screenBitmap;
	XImage* backBuffer = createXImage(display, &visualInfo, windowWidth, windowHeight, &screenBitmap);

    GC graphicsContext = DefaultGC(display, screenNum);

    void* memory = calloc(GIGABYTE(1), 1);
	*((char**)memory) = rayFileName;

    Input input = {};
    input.mouse.leftClickStartPos = V2(-1, -1);
    input.mouse.rightClickStartPos = V2(-1, -1);
    Mouse oldMouse = {};

	PlatformFunctions platformFunctions = {};
	platformFunctions.createThread = createThread;
	platformFunctions.acquireLock = acquireLock;
	platformFunctions.releaseLock = releaseLock;
	
	f64 secondsPerFrame = (f64)1/(f64)60;
	u64 nanoSecondsPerFrame = (u64)(secondsPerFrame*1000000000L);
	
    timespec clockTime;
    //TODO(denis): can use CLOCK_REALTIME instead?
    //NOTE(denis): CLOCK_REALTIME is affected by the user manually changing the system time
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &clockTime);

	appInit((Memory*)memory, platformFunctions);
	
    bool running = true;
    while(running)
    {
		XEvent event;
		while(XPending(display))
		{
			XNextEvent(display, &event);

			switch(event.type)
			{
				case ConfigureNotify:
				{
					u32 newWidth = event.xconfigure.width;
					u32 newHeight = event.xconfigure.height;

					if (screenBitmap.width != newWidth || screenBitmap.height != newHeight)
					{
						windowWidth = newWidth;
						windowHeight = newHeight;

						XDestroyImage(backBuffer);
						backBuffer = createXImage(display, &visualInfo, newWidth, newHeight, &screenBitmap);
					}
				} break;
				
				case KeyPress:
				{
#if 0
					char keyText[255];
					KeySym key;
					XLookupString(&event.xkey, keyText, 255, &key, 0);

		    
					if (keyText[0] == 'q')
					{
						running = false;
					}
#endif
		    
					//TODO(denis): should I use keyText instead of these calls? might be faster?
					if (event.xkey.keycode == XKeysymToKeycode(display, XK_Left))
					{
						input.controller.leftPressed = true;
					}
					else if (event.xkey.keycode == XKeysymToKeycode(display, XK_Right))
					{
						input.controller.rightPressed = true;
					}
					else if (event.xkey.keycode == XKeysymToKeycode(display, XK_Up))
					{
						input.controller.upPressed = true;
					}
					else if (event.xkey.keycode == XKeysymToKeycode(display, XK_Down))
					{
						input.controller.downPressed = true;
					}

					if (event.xkey.keycode == XKeysymToKeycode(display, XK_space))
					{
						input.controller.actionPressed = true;
					}
				} break;

				case KeyRelease:
				{
					if (event.xkey.keycode == XKeysymToKeycode(display, XK_Left))
					{
						input.controller.leftPressed = false;
					}
					else if (event.xkey.keycode == XKeysymToKeycode(display, XK_Right))
					{
						input.controller.rightPressed = false;
					}
					else if (event.xkey.keycode == XKeysymToKeycode(display, XK_Up))
					{
						input.controller.upPressed = false;
					}
					else if (event.xkey.keycode == XKeysymToKeycode(display, XK_Down))
					{
						input.controller.downPressed = false;
					}
		    
					if (event.xkey.keycode == XKeysymToKeycode(display, XK_space))
					{
						input.controller.actionPressed = false;
					}
				} break;

				case ButtonPress:
				{
					if (event.xbutton.button == Button1)
					{
						input.mouse.leftPressed = true;
						input.mouse.leftWasPressed = false;
						input.mouse.leftClickStartPos = V2(event.xbutton.x, event.xbutton.y);
					}
					else if (event.xbutton.button == Button3)
					{
						input.mouse.rightPressed = true;
						input.mouse.rightWasPressed = false;
						input.mouse.rightClickStartPos = V2(event.xbutton.x, event.xbutton.y);
					}
				} break;

				case ButtonRelease:
				{
					if (event.xbutton.button == Button1)
					{
						input.mouse.leftPressed = false;
						input.mouse.leftWasPressed = true;
					}
					else if (event.xbutton.button == Button3)
					{
						input.mouse.rightPressed = false;
						input.mouse.rightWasPressed = true;
					}
				} break;

				// mouse movement event
				case MotionNotify:
				{
					input.mouse.pos.x = event.xmotion.x;
					input.mouse.pos.y = event.xmotion.y;

					//TODO(denis): do I want to look at event.xmotion.state?
				} break;

				case DestroyNotify:
				{
					XDestroyWindowEvent* e = (XDestroyWindowEvent*)&event;
					if (e->window == window)
						running = false;
				} break;
			}
		}

		mainUpdateCall((Memory*)memory, &screenBitmap, &input, platformFunctions);
	
		XPutImage(display, window, graphicsContext, backBuffer, 0, 0,
				  0, 0, windowWidth, windowHeight);
		//TODO(denis): might need to do XFlush(display); here every frame to be sure?

		//TODO(denis): only allows programs to respond to a mouse click that just ended for one frame
		bool disableLeftWasPressed = oldMouse.leftPressed && input.mouse.leftWasPressed;
		bool disableRightWasPressed = oldMouse.rightPressed && input.mouse.rightWasPressed;

		oldMouse = input.mouse;
		if (disableLeftWasPressed)
		{
			input.mouse.leftWasPressed = false;
			input.mouse.leftClickStartPos = V2(-1, -1);
		}
		else if (disableRightWasPressed)
		{
			input.mouse.rightWasPressed = false;
			input.mouse.rightClickStartPos = V2(-1, -1);
		}
	
		timespec endTime;
		long nanoDiff = getElapsedTimeNs(&clockTime, &endTime);

		//TODO(denis): busy loop, not good!
		while ((u64)nanoDiff < nanoSecondsPerFrame)
		{
			nanoDiff = getElapsedTimeNs(&clockTime, &endTime);
		}

#if 0
		long elapsedMs = nanoDiff / 1000000;
		printf("time diff: %ld ms\n", elapsedMs);
#endif
	
		clockTime = endTime;
    }

    free(memory);
    
    return 0;
}
