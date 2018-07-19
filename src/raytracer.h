#if !defined(RAYTRACER_H_)
#define RAYTRACER_H_

struct RaytracerSettings
{
	PlatformFunctions platform;
	
	bool raytracerComplete;
	u32 bufferLock;

	Bitmap resultBuffer;
	Bitmap workBuffer;

	Scene scene;
};

#endif
