#define NOMINMAX
#define Rectangle _Rectangle
#include <windows.h>
#undef Rectangle
#undef near
#undef far
#include "Strsafe.h"
#include "Windowsx.h"

#include "denis_types.h"
#include "denis_math.h"

#include "platform_layer.h"
#include "shared_platform_functions.h"

#define DEFAULT_WINDOW_WIDTH 640
#define DEFAULT_WINDOW_HEIGHT 480

struct Memory;
typedef MAIN_UPDATE_CALL(*mainUpdateCallPtr);

extern MAIN_UPDATE_CALL(mainUpdateCall);
extern APP_INIT_CALL(appInit);

struct BackBuffer
{
    void* data;
    BITMAPINFO bitmapInfo;
};

static bool _running = true;
static HDC _deviceContext;
static u32 _windowWidth;
static u32 _windowHeight;
static u32 _currentTouchPoint;

static BackBuffer _backBuffer;
static Input _input;

struct WindowsThread
{
	void(*function)(void*);
	void* data;
};

static WindowsThread _windowsThread;

//NOTE(denis): pass through function to convert windows style thread function to our application thread
// style
DWORD thread(LPVOID data)
{
	WindowsThread* windowsThread = (WindowsThread*)data;
	windowsThread->function(windowsThread->data);

	return 0;
}

void createThread(void(*threadFunction)(void*), void* data)
{
	_windowsThread.function = threadFunction;
	_windowsThread.data = data;

	HANDLE newThread;
    newThread = CreateThread(NULL, 0, thread, &_windowsThread, 0, NULL);

	//TODO(denis): should  kill the thread after exiting the main loop
	// TerminateThread(handle, exitCode) in win32
}

//TODO(denis): these are busy loops, which isn't fantastic
inline void acquireLock(volatile u32* lock)
{
	while (InterlockedExchange(lock, 1) == 1)
		;
}
inline void releaseLock(volatile u32* lock)
{
	while (InterlockedExchange(lock, 0) == 0)
		;
}

LRESULT CALLBACK win32_messageCallback(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
	
    switch(message)
    {
		case WM_DESTROY:
		{
			_running = false;
		} break;

		case WM_QUIT:
		{
			_running = false;
		} break;

		case WM_SIZE:
		{
			RECT clientRect;
			GetClientRect(windowHandle, &clientRect);
			_windowWidth = clientRect.right - clientRect.left;
			_windowHeight = clientRect.bottom - clientRect.top;

			if (_backBuffer.data)
			{
				HEAP_FREE(_backBuffer.data);
			}

			_backBuffer.bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			_backBuffer.bitmapInfo.bmiHeader.biWidth = clientRect.right;
			_backBuffer.bitmapInfo.bmiHeader.biHeight = -clientRect.bottom; //NOTE(denis): positive means origin in lower-left, negative means origin in upper-left
			_backBuffer.bitmapInfo.bmiHeader.biPlanes = 1;
			_backBuffer.bitmapInfo.bmiHeader.biBitCount = 32;
			_backBuffer.bitmapInfo.bmiHeader.biCompression = BI_RGB;

			//TODO(denis): error checking?
			_backBuffer.data = HEAP_ALLOC(_windowWidth*_windowHeight*(_backBuffer.bitmapInfo.bmiHeader.biBitCount/8));
		} break;

		case WM_PAINT:
		{
			StretchDIBits(_deviceContext, 0, 0, _windowWidth, _windowHeight,
						  0, 0, _backBuffer.bitmapInfo.bmiHeader.biWidth, ABS_VALUE(_backBuffer.bitmapInfo.bmiHeader.biHeight),
						  _backBuffer.data, &_backBuffer.bitmapInfo, DIB_RGB_COLORS, SRCCOPY);

			RECT windowRect = { 0, 0, (LONG)_windowWidth, (LONG)_windowHeight };
			ValidateRect(windowHandle, &windowRect);
		} break;

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			if (wParam == VK_UP)
			{
				_input.controller.upPressed = true;
			}
			else if (wParam == VK_DOWN)
			{
				_input.controller.downPressed = true;
			}
			else if (wParam == VK_LEFT)
			{
				_input.controller.leftPressed = true;
			}
			else if (wParam == VK_RIGHT)
			{
				_input.controller.rightPressed = true;
			}

			if (wParam == VK_SPACE)
			{
				_input.controller.actionPressed = true;
			}
		} break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			if (wParam == VK_UP)
			{
				_input.controller.upPressed = false;
			}
			else if (wParam == VK_DOWN)
			{
				_input.controller.downPressed = false;
			}
			else if (wParam == VK_LEFT)
			{
				_input.controller.leftPressed = false;
			}
			else if (wParam == VK_RIGHT)
			{
				_input.controller.rightPressed = false;
			}

			if (wParam == VK_SPACE)
			{
				_input.controller.actionPressed = false;
			}
		} break;

		case WM_MOUSEMOVE:
		{
		    v2 mousePos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			_input.mouse.pos = mousePos;
		} break;

		case WM_LBUTTONDOWN:
		{
		    v2 mousePos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			_input.mouse.pos = mousePos;
			_input.mouse.leftClickStartPos = mousePos;

			_input.mouse.leftWasPressed = false;
			_input.mouse.leftPressed = true;
		} break;

		case WM_LBUTTONUP:
		{
		    v2 mousePos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			_input.mouse.pos = mousePos;

			_input.mouse.leftWasPressed = true;
			_input.mouse.leftPressed = false;
		} break;

		case WM_RBUTTONDOWN:
		{
		    v2 mousePos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			_input.mouse.pos = mousePos;
			_input.mouse.rightClickStartPos = mousePos;

			_input.mouse.rightWasPressed = false;
			_input.mouse.rightPressed = true;
		} break;

		case WM_RBUTTONUP:
		{
		    v2 mousePos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
			_input.mouse.pos = mousePos;

			_input.mouse.rightWasPressed = true;
			_input.mouse.rightPressed = false;
		} break;
		
		//NOTE(denis): used for touch and pen input
		
		case WM_POINTERUPDATE:
		{
			u32 pointerID = GET_POINTERID_WPARAM(wParam);
			POINTER_INPUT_TYPE inputType;
			
			if (GetPointerType(pointerID, &inputType))
			{
				switch (inputType)
				{
					case PT_TOUCH:
					{
						//TODO(denis): for now we only process up to five points
						if (_currentTouchPoint >= 4)
							break;
						
						POINTER_TOUCH_INFO touchInfo = {};
						GetPointerTouchInfo(pointerID, &touchInfo);

						RECT touchRect = touchInfo.rcContactRaw;
						POINT touchPoint;
						if (touchInfo.touchMask & TOUCH_MASK_CONTACTAREA)
						{
							touchPoint.x =
								touchRect.left + (touchRect.right - touchRect.left)/2;
							touchPoint.y =
								touchRect.top + (touchRect.bottom - touchRect.top)/2;
						}
						else
						{
							touchPoint.x = touchRect.left;
							touchPoint.y = touchRect.top;
						}

						ScreenToClient(windowHandle, &touchPoint);
						_input.touch.points[_currentTouchPoint].x = touchPoint.x;
						_input.touch.points[_currentTouchPoint].y = touchPoint.y;
						
						++_currentTouchPoint;
						_input.touch.numActivePoints = _currentTouchPoint;
					} break;

					case PT_PEN:
					{
						POINTER_PEN_INFO penInfo = {};
						GetPointerPenInfo(pointerID, &penInfo);

						POINT penPos = penInfo.pointerInfo.ptPixelLocationRaw;
						ScreenToClient(windowHandle, &penPos);

						_input.pen.x = penPos.x;
						_input.pen.y = penPos.y;

						_input.pen.usingEraser = penInfo.penFlags & PEN_FLAG_ERASER;
						
						if (penInfo.penMask & PEN_MASK_PRESSURE)
						{
							_input.pen.pressure = penInfo.pressure;
						}
						else
						{
							_input.pen.pressure = 0;
						}
					} break;
				}
			}
		} break;
		
		default:
		{
			result = DefWindowProc(windowHandle, message, wParam, lParam);
		} break;
    }

    return result;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmdLine, int cmdShow)
{
	u32 windowWidth = DEFAULT_WINDOW_WIDTH;
	u32 windowHeight = DEFAULT_WINDOW_HEIGHT;

	char* rayFileName = 0;
	
	if (cmdLine[0] != 0)
	{
		getWindowSizeFromCommandLine(cmdLine, &windowWidth, &windowHeight);
		getRayFileFromCommandLine(cmdLine, &rayFileName);
	}

	if (rayFileName == 0)
		rayFileName = "default_scene.ray";
	
    WNDCLASSEX windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = win32_messageCallback;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(0, IDC_ARROW);
    windowClass.lpszClassName = "win32WindowClass";
	
    if (!RegisterClassEx(&windowClass))
    {
		OutputDebugString("Error creating window class\n");
		return 1;
    }

    DWORD windowStyles = WS_OVERLAPPEDWINDOW|WS_VISIBLE;

    RECT windowRect = {0, 0, (LONG)windowWidth, (LONG)windowHeight};
    AdjustWindowRectEx(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0);
	
    HWND windowHandle =
		CreateWindowEx(0, windowClass.lpszClassName, "Raytracer",
					   windowStyles,
					   CW_USEDEFAULT, CW_USEDEFAULT,
					   windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
					   0, 0, instance, 0);
	
    if (!windowHandle)
    {
		OutputDebugString("Error creating window\n");
		return 1;
    }

    //NOTE(denis): don't need to release this because this is our window's private DC
    _deviceContext = GetDC(windowHandle);
	
    //TODO(denis): should probably let the user set the size of this
    void* mainMemory = VirtualAlloc(0, GIGABYTE(1), MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
	*((char**)mainMemory) = rayFileName;
	
    LARGE_INTEGER countFrequency;
    QueryPerformanceFrequency(&countFrequency); //NOTE(denis): counts/second
	
    LARGE_INTEGER lastCounts;
    QueryPerformanceCounter(&lastCounts); //NOTE(denis): counts

    //TODO(denis): might want to do this eventually
    //SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);

    Mouse oldMouse = {};
    _input.mouse.leftClickStartPos = V2(-1, -1);
    _input.mouse.rightClickStartPos = V2(-1, -1);

	PlatformFunctions platformFunctions = {};
	platformFunctions.createThread = createThread;
	platformFunctions.acquireLock = acquireLock;
	platformFunctions.releaseLock = releaseLock;

	appInit((Memory*)mainMemory, platformFunctions);
	
    while (_running)
    {
		MSG message;
		while (PeekMessage(&message, windowHandle, 0, 0, PM_REMOVE))
		{
			// NOTE(denis): this is here because sometimes messages aren't dispatched properly for some reason
			if (message.message == WM_QUIT)
				_running = false;
		        
			TranslateMessage(&message);
			DispatchMessage(&message);
		}
		
		Bitmap screen;
		screen.pixels = (u32*)_backBuffer.data;
		screen.width = _backBuffer.bitmapInfo.bmiHeader.biWidth;
		screen.height = ABS_VALUE(_backBuffer.bitmapInfo.bmiHeader.biHeight);
		
		mainUpdateCall((Memory*)mainMemory, &screen, &_input, platformFunctions);
		
		RedrawWindow(windowHandle, 0, 0, RDW_INTERNALPAINT);

		LARGE_INTEGER currentCounts;
		QueryPerformanceCounter(&currentCounts);
		u64 timePassed = currentCounts.QuadPart - lastCounts.QuadPart;
		f64 timeMs = (f64)timePassed * 1000.0 / (f64)countFrequency.QuadPart;

		//TODO(denis): probably don't do a busy loop
		//NOTE(denis): the epsilon is an attempt to lessen the effects of random spikes
		f32 epsilon = 0.01f;
		while (timeMs < (f64)1/(f64)60 * 1000.0 - epsilon)
		{
			QueryPerformanceCounter(&currentCounts);
			timePassed = currentCounts.QuadPart - lastCounts.QuadPart;
			timeMs = (f64)timePassed * 1000.0 / (f64)countFrequency.QuadPart;
		}
#if 0
		char timeBuffer[100];
		StringCbPrintf(timeBuffer, 100, "%f\n", timeMs);
		OutputDebugString(timeBuffer);
#endif
		lastCounts = currentCounts;

		_currentTouchPoint = 0;
		_input.touch = {};

		//TODO(denis): this only gives programs one frame to handle mouse clicks
		// is that enough? It seems like it should be fine, but maybe it would be safer with
		// more than one frame? But then users would need to add tests to avoid multiple clicks
		// registering in a program
		bool disableLeftWasPressed = _input.mouse.leftWasPressed && oldMouse.leftPressed;
		bool disableRightWasPressed = _input.mouse.rightWasPressed && oldMouse.rightPressed;

		oldMouse = _input.mouse;
		if (disableLeftWasPressed)
		{
			_input.mouse.leftWasPressed = false;
			_input.mouse.leftClickStartPos = V2(-1, -1);
		}
		if (disableRightWasPressed)
		{
			_input.mouse.rightWasPressed = false;
			_input.mouse.rightClickStartPos = V2(-1, -1);
		}
    }
	
    DestroyWindow(windowHandle);
	
    return 0;
}
