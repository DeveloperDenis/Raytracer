#if !defined(SHARED_PLATFORM_FUNCTIONS_H_)
#define SHARED_PLATFORM_FUNCTIONS_H_

#include "denis_strings.h"

bool getWindowSizeFromCommandLine(char* commandLine, u32* windowWidth, u32* windowHeight)
{
	bool success = true;
	
	u32 width = 0;
	u32 height = 0;
	bool firstValue = true;
	for (u32 i = 0; commandLine[i] != 0 && success; ++i)
	{
		if (commandLine[i] >= '0' && commandLine[i] <= '9')
		{
			if (firstValue)
			{
				width *= 10;
				width += commandLine[i] - '0';
			}
			else
			{
				height *= 10;
				height += commandLine[i] - '0';
			}
		}
		else if (commandLine[i] == 'x' || commandLine[i] == 'X')
		{
			if (firstValue)
				firstValue = false;
			else
			{
				success = false;
			}
		}
		else
		{
			//reached the end of the command line argument we care about, but there are trailing spaces
			if (!firstValue && height != 0 && commandLine[i] == ' ')
				break;
		}
	}

	if (firstValue || height == 0)
	{
		success = false;
	}
	else
	{
		*windowWidth = width;
		*windowHeight = height;
	}

	return success;
}

//NOTE: the .ray file name must start with a letter
bool getRayFileFromCommandLine(char* commandLine, char** rayFileName)
{
	bool success = false;

	bool startOfRayFile = false;
	bool extensionReached = false;
	for (u32 i = 0; commandLine[i] != 0; ++i)
	{
		if (startOfRayFile)
		{
			if (commandLine[i] == ' ' && !extensionReached)
			{
				startOfRayFile = false;
				*rayFileName = 0;
			}
			else if (commandLine[i] == '.')
			{
				extensionReached = true;
				if (commandLine[i+1] == 0 || commandLine[i+2] == 0 || commandLine[i+3] == 0)
				{
					extensionReached = false;
					startOfRayFile = false;
					*rayFileName = 0;
					continue;
				}

				//TODO(denis): doesn't allow upper case extensions
				if (commandLine[i+1] == 'r' && commandLine[i+2] == 'a' && commandLine[i+3] == 'y')
				{
					if (commandLine[i+4] == ' ' || commandLine[i+4] == 0)
					{
						(*rayFileName)[i+4] = 0;
						success = true;
						break;
					}
					else
					{
						extensionReached = false;
						startOfRayFile = false;
						*rayFileName = 0;
						continue;						
					}
				}
			}
		}
		else if (IS_UPPER_CASE(commandLine[i]) || IS_LOWER_CASE(commandLine[i]))
		{
			startOfRayFile = true;
			*rayFileName = &commandLine[i];
		}
	}

	return success;
}

#endif
