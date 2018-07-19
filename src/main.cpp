#include <float.h>
#include "platform_layer.h"
#include "denis_drawing.h"
#include "ray_file_parser.h"

#include "raytracer.cpp"

#define BUTTON_COLOUR 0xFF73d127
#define BUTTON_COLOUR_ACTIVE 0xFF396615

#define PROGRESS_CONTAINER_COLOUR 0xFFCCCCCC
#define PROGRESS_BAR_COLOUR_OUTER BUTTON_COLOUR_ACTIVE
#define PROGRESS_BAR_COLOUR_INNER BUTTON_COLOUR

struct Memory
{
	char* rayFileName;

	bool drawUI;
	
	u32 totalPixels;
	u32 currentPixel;
	
	bool raytraceInProgress;
	// this holds the result of the raytracer's execution, needs to be at the end of the memory struct
	// so that the buffer can be any size
    RaytracerSettings raytracerSettings;
};

APP_INIT_CALL(appInit)
{
	RaytracerSettings* raytracerSettings = &memory->raytracerSettings;

	raytracerSettings->scene = parseRayFile(memory->rayFileName);
	
	memory->drawUI = true;
	memory->raytraceInProgress = false;

	raytracerSettings->platform = platformFunctions;

	raytracerSettings->raytracerComplete = false;
	raytracerSettings->bufferLock = 0;
	raytracerSettings->resultBuffer.pixels = (u32*)(&memory->raytracerSettings + 1);
}

MAIN_UPDATE_CALL(mainUpdateCall)
{
	RaytracerSettings* raytracerSettings = &memory->raytracerSettings;

	// setting up UI sizes
	f32 progressRatio = (f32)memory->currentPixel / (f32)memory->totalPixels;
	
	u32 containerHeight = 35;
	u32 containerY = screen->height - containerHeight;

	u32 buttonWidth = screen->width/5;
	u32 buttonHeight = (u32)(buttonWidth * 0.4f);
	Rect2 button = Rect2(screen->width/2 - buttonWidth/2, 10,
						 buttonWidth, buttonHeight);
	u32 buttonColour = BUTTON_COLOUR;

	// logic
	
	if (raytracerSettings->raytracerComplete)
	{
		memory->raytraceInProgress = false;
		fillBuffer(&raytracerSettings->workBuffer, 0x00000000);
	}
	
	if (!memory->raytraceInProgress)
	{
		if (memory->drawUI && input->mouse.leftWasPressed && pointInRect(input->mouse.pos, button))
		{
			memory->raytraceInProgress = true;
			
		    raytracerSettings->raytracerComplete = false;
		    raytracerSettings->workBuffer.width = screen->width;
		    raytracerSettings->workBuffer.height = screen->height;
			memory->totalPixels = screen->width*screen->height;
			memory->currentPixel = 0;

			//NOTE(denis): the work buffer's starting location is pushed further and further as
			// the size of the window increases, but we never shrink it again because we don't
			// want the work buffer's pixel data to overlap with the result buffer.
			// Never shrinking shouldn't be a problem since users won't (typically, unless they were
			// really trying to mess this up) resize a window past a certain maximum size
			if (raytracerSettings->workBuffer.width * raytracerSettings->workBuffer.height >
				raytracerSettings->resultBuffer.width * raytracerSettings->resultBuffer.height)
			{
				raytracerSettings->workBuffer.pixels = (u32*)(raytracerSettings + 1) + (screen->width*screen->height);
			}

			platformFunctions.createThread(raytracer, raytracerSettings);
		}
		if (memory->drawUI && input->mouse.leftPressed && pointInRect(input->mouse.pos, button))
		{
			buttonColour = BUTTON_COLOUR_ACTIVE;
		}
	}

	for (u32 i = 0; i < memory->totalPixels; ++i)
	{
		memory->currentPixel = i;
		if (raytracerSettings->workBuffer.pixels[i] == 0)
			break;
	}

	if (input->mouse.rightWasPressed)
		memory->drawUI = !memory->drawUI;
	
	// drawing

	fillBuffer(screen, 0xFF333333);
    
	platformFunctions.acquireLock(&raytracerSettings->bufferLock);
	drawBitmap(screen, &raytracerSettings->resultBuffer, V2(0,0));
	platformFunctions.releaseLock(&raytracerSettings->bufferLock);

	// draw progress bar

	if (memory->drawUI)
	{
		drawRect(screen, 0, containerY, screen->width, containerHeight, PROGRESS_CONTAINER_COLOUR);

		u32 padding = 5;
		u32 barHeight = containerHeight - padding*2;
		u32 barWidth = (u32)(screen->width*progressRatio) - padding*2;
		u32 barY = containerY + padding;;
		u32 barX = padding;
		drawRect(screen, barX, barY, screen->width - padding*2, barHeight, PROGRESS_BAR_COLOUR_OUTER);

		drawRect(screen, barX+2, barY+2, barWidth - 4, barHeight - 4, PROGRESS_BAR_COLOUR_INNER);

		if (!memory->raytraceInProgress)
		{
			drawRect(screen, button, PROGRESS_CONTAINER_COLOUR);

			u32 innerPadding = buttonHeight/10;

			u32 x = button.getLeft() + innerPadding;
			u32 y = button.getTop() + innerPadding;
			u32 width = button.getWidth() - innerPadding*2;
			u32 height = button.getHeight() - innerPadding*2;
		
			drawRect(screen, x, y, width, height, buttonColour);
		}
	}
}
