#if !defined(DENIS_DRAWING_H_)
#define DENIS_DRAWING_H_

#include "denis_math.h"

//NOTE(denis): draws the src buffer into the dest buffer with x and y specified in rect
// clips bitmap width and height to rect width & rect height
//TODO(denis): doesn't draw partial alpha, only full or none
static void drawBitmap(Bitmap* destBitmap, Bitmap* srcBitmap, Rect2 rect)
{
	s32 startY = MAX(rect.getTop(), 0);
	s32 endY = MIN(rect.getBottom(), (s32)destBitmap->height);

	s32 startX = MAX(rect.getLeft(), 0);
	s32 endX = MIN(rect.getRight(), (s32)destBitmap->width);

	u32 row = 0;
	for (s32 y = startY; y < endY && row < srcBitmap->height; ++y, ++row)
	{
		u32 col = 0;
		for (s32 x = startX; x < endX && col < srcBitmap->width; ++x, ++col)
		{
			u32* inPixel = srcBitmap->pixels + row*srcBitmap->width + col;
			u32* outPixel = destBitmap->pixels + y*destBitmap->width + x;

			if (((*inPixel) & (0xFF << 24)) != 0)
			{
				*outPixel = *inPixel;
			}
		}
	}
}
static inline void drawBitmap(Bitmap* destBitmap, Bitmap* srcBitmap, v2 pos)
{
	Rect2 rect = Rect2(pos.x, pos.y, srcBitmap->width, srcBitmap->height);
	drawBitmap(destBitmap, srcBitmap, rect);
}

static inline void fillBuffer(Bitmap* buffer, u32 colour)
{
    for (u32 row = 0; row < buffer->height; ++row)
    {
	for (u32 col = 0; col < buffer->width; ++col)
	{
	    *(buffer->pixels + col + row*buffer->width) = colour;
	}
    }
}

//NOTE(denis): (x, y) is the top left of the rectangle
static void drawRect(Bitmap* buffer,
		     s32 x, s32 y, u32 width, u32 height, u32 colour)
{
    if (x + (s32)width < 0 || y + (s32)height < 0)
	return;
    
    u32 startX = MAX(0, x);
    u32 startY = MAX(0, y);

    u32 endX = MIN(buffer->width, x + width);
    u32 endY = MIN(buffer->height, y + height);
	
    for (u32 row = startY; row < endY; ++row)
    {
	for (u32 col = startX; col < endX; ++col)
	{
	    *(buffer->pixels + row*buffer->width + col) = colour;
	}
    }
}
static inline void drawRect(Bitmap* buffer, Rect2 rect, u32 colour)
{
    drawRect(buffer, rect.getLeft(), rect.getTop(), rect.getWidth(), rect.getHeight(), colour);
}

//NOTE(denis): (x, y) is the centre of the circle
static void drawCircle(Bitmap* buffer, s32 x, s32 y, s32 radius, u32 colour)
{
    if (x + radius < 0 || y + radius < 0)
	return;
    
    s32 startX = MAX(x - radius, 0);
    s32 startY = MAX(y - radius, 0);

    s32 endX = MIN(x + radius, (s32)buffer->width);
    s32 endY = MIN(y + radius, (s32)buffer->height);

    for (s32 row = startY; row < endY; ++row)
    {
	for (s32 col = startX; col < endX; ++col)
	{
	    s32 xDiff = col - x;
	    s32 yDiff = row - y;

	    if (sqrt(xDiff*xDiff + yDiff*yDiff) <= (f32)radius)
	    {
		*(buffer->pixels + row*buffer->width + col) = colour;
	    }
	}
    }
}
static inline void drawCircle(Bitmap* buffer, v2 pos, s32 radius, u32 colour)
{
	drawCircle(buffer, pos.x, pos.y, radius, colour);
}

static inline void drawPoint(Bitmap* buffer, s32 x, s32 y, u32 colour)
{
	if (x < 0 || x >= (s32)buffer->width || y < 0 || y >= (s32)buffer->height)
		return;

	*(buffer->pixels + y*buffer->width + x) = colour;
}

//TODO(denis): some things need to be fixed/added
// - add line width parameter
// - for line widths, need to take into account the slope when deciding how to widen line
// - also this is probably overly verbose and inefficient
// if this is deemed too slow, look into "Bresenham's line algorithm"
static void drawLine(Bitmap* buffer, v2 point1, v2 point2, u32 colour)
{
    v2 startPoint;
    v2 endPoint;
	
    s32 rise = point2.y - point1.y;
    s32 run = point2.x - point1.x;

    f32 slope;
	
    if (ABS_VALUE(rise) > ABS_VALUE(run))
    {
	if (point1.y < point2.y)
	{
	    startPoint = point1;
	    endPoint = point2;
	}
	else
	{
	    startPoint = point2;
	    endPoint = point1;
	}

	if (startPoint.y >= (s32)buffer->height || endPoint.y < 0)
	    return;

	u32 startY = MAX(startPoint.y, 0);
	u32 endY = MIN(endPoint.y, (s32)buffer->height);

	rise = endPoint.y - startPoint.y;
	run = endPoint.x - startPoint.x;

	if (run == 0)
	    slope = 1.0f;
	else
	    slope = (f32)rise/(f32)run;
		
	for (u32 y = startY; y < endY; ++y)
	{
	    s32 x;
	    if (slope == 1.0f)
	    {
		x = startPoint.x;
	    }
	    else
	    {
		x = (s32)((f32)(y - startPoint.y)/slope) + startPoint.x;
	    }

	    if (x > 0 && x < (s32)buffer->width)
		*(buffer->pixels + y*buffer->width + x) = colour;
	}
    }
    else
    {
	if (point1.x < point2.x)
	{
	    startPoint = point1;
	    endPoint = point2;
	}
	else
	{
	    startPoint = point2;
	    endPoint = point1;
	}

	if (startPoint.x >= (s32)buffer->width || endPoint.x < 0)
	    return;

	u32 startX = MAX(startPoint.x, 0);
	u32 endX = MIN(endPoint.x, (s32)buffer->width);

	rise = endPoint.y - startPoint.y;
	run = endPoint.x - startPoint.x;

	slope = (f32)rise/(f32)run;
		
	for (u32 x = startX; x < endX; ++x)
	{
	    s32 y = (s32)(slope*(x - startPoint.x)) + startPoint.y;

	    if (y > 0 && y < (s32)buffer->height)
		*(buffer->pixels + y*buffer->width + x) = colour;
	}
    }
}

#endif
