#include "raytracer.h"

union Triangle
{
	struct
	{
		v3f p1;
		v3f p2;
		v3f p3;
	};
	v3f points[3];

	v3f& operator[](u32 index)
	{
		return points[index];
	}
};

static inline Triangle getTriangleFromFace(v3* face, v3f* vertices)
{
	Triangle result = {vertices[face->x], vertices[face->y], vertices[face->z]};
	return result;
}

static inline void swap(f32* a, f32* b)
{
	f32 temp = *a;
	*a = *b;
	*b = temp;
}
//NOTE(denis): assumes colour vector has values in the range [0, 1]
static inline u32 convertColour(v3f colour)
{
	u32 result;
	result = (0xFF << 24) | ((u8)(colour.r*0xFF) << 16) | ((u8)(colour.g*0xFF) << 8) | ((u8)(colour.b*0xFF));
	return result;
}

// does a linear interpolation between the three given vectors
static inline v3f interpolate(v3f v0, v3f v1, v3f v2, v3f ratios)
{
	return v0*ratios[0] + v1*ratios[1] + v2*ratios[2];
}

static inline bool isValidIntersection(f32 tValue)
{
	return tValue != FLT_MAX && tValue != FLT_MIN && tValue > 0.0f;
}

static inline bool isValidFace(v3* face, u32 numVertices)
{
	return face->x < (s32)numVertices && face->y < (s32)numVertices && face->z < (s32)numVertices;
}

// INTERSECTION FUNCTIONS
static f32 intersectPlane(Plane* plane, v3f rayOrigin, v3f rayDirection)
{
	f32 t = FLT_MAX;

	f32 denominator = dot(plane->normal, rayDirection);
	f32 nominator = -plane->distance - dot(plane->normal, rayOrigin);

	if (denominator != 0.0f)
	{
	    t = nominator / denominator;
	}
	else
	{
		t = nominator < 0.0f ? FLT_MIN : FLT_MAX;
	}

	return t;
}

static f32 intersectSphere(Sphere* sphere, v3f rayOrigin, v3f rayDirection)
{
	f32 t = FLT_MAX;
	
	v3f rayRelativeOrigin = rayOrigin - sphere->pos;

	f32 a = 1.0f; // this is because rayDirection is a normalized vector
	f32 b = 2.0f*dot(rayRelativeOrigin, rayDirection);
	f32 c = dot(rayRelativeOrigin, rayRelativeOrigin) - sphere->radius*sphere->radius;

	f32 discriminant = b*b - 4.0f*a*c;
	if (discriminant >= 0.0f)
	{
		f32 negativeT = (-b - sqrt(discriminant)) / (2.0f*a);
		f32 positiveT = (-b + sqrt(discriminant)) / (2.0f*a);

		t = negativeT;
		if (negativeT < 0.0f && positiveT > 0.0f)
			t = positiveT;
	}
	
	return t;
}

//NOTE(denis): box intersection using the Kay-Kajiya bounding volume ("slab") method
static f32 intersectBox(Box* box, v3f rayOrigin, v3f rayDirection, v3f* intersectNormal)
{
	f32 t = FLT_MAX;
	
	v3f xNormal = V3f(1.0f, 0.0f, 0.0f);
	Plane xPlaneNear, xPlaneFar;
	xPlaneNear.normal = xPlaneFar.normal = xNormal;
	xPlaneNear.distance = box->min.x;
	xPlaneFar.distance = box->max.x;

	v3f yNormal = V3f(0.0f, 1.0f, 0.0f);
	Plane yPlaneNear, yPlaneFar;
	yPlaneNear.normal = yPlaneFar.normal = yNormal;
	yPlaneNear.distance = box->min.y;
	yPlaneFar.distance = box->max.y;

	v3f zNormal = V3f(0.0f, 0.0f, 1.0f);
	Plane zPlaneNear, zPlaneFar;
	zPlaneNear.normal = zPlaneFar.normal = zNormal;
	zPlaneNear.distance = box->min.z;
	zPlaneFar.distance = box->max.z;

	f32 tNearX = intersectPlane(&xPlaneNear, rayOrigin, rayDirection);
	f32 tFarX = intersectPlane(&xPlaneFar, rayOrigin, rayDirection);

	f32 tNearY = intersectPlane(&yPlaneNear, rayOrigin, rayDirection);
	f32 tFarY = intersectPlane(&yPlaneFar, rayOrigin, rayDirection);

	f32 tNearZ = intersectPlane(&zPlaneNear, rayOrigin, rayDirection);
	f32 tFarZ = intersectPlane(&zPlaneFar, rayOrigin, rayDirection);
    
	if ((tNearX <= 0.0f && tFarX <= 0.0f) ||
		(tNearY <= 0.0f && tFarY <= 0.0f) ||
		(tNearZ <= 0.0f && tFarZ <= 0.0f))
	{
		return t;
	}

	if (tNearX > tFarX)
		swap(&tNearX, &tFarX);
	if (tNearY > tFarY)
		swap(&tNearY, &tFarY);
	if (tNearZ > tFarZ)
		swap(&tNearZ, &tFarZ);
				
	f32 tNear = MAX(MAX(tNearX, tNearY), tNearZ);
	f32 tFar = MIN(MIN(tFarX, tFarY), tFarZ);

	if (tNear < tFar)
	{
		t = tNear;

		if (isValidIntersection(t) && intersectNormal)
		{
			v3f rayPoint = rayOrigin + t*rayDirection;
			
			if (tNear == tNearX || tNear == tFarX)
			{
				*intersectNormal = xNormal;
				
				if (rayPoint.x < box->max.x)
					intersectNormal->x = -intersectNormal->x;
			}
			else if (tNear == tNearY || tNear == tFarY)
			{
			    *intersectNormal = yNormal;
				
				if (rayPoint.y < box->max.y)
					intersectNormal->y = -intersectNormal->y;
			}
			else if (tNear == tNearZ || tNear == tFarZ)
			{
			    *intersectNormal = zNormal;
				
				if (rayPoint.z < box->max.z)
				    intersectNormal->z = -intersectNormal->z;
			}
		}
	}

	return t;
}

static f32 intersectTriangle(Triangle triangle, v3f rayOrigin, v3f rayDirection, v3f* barycentric)
{
	f32 t = FLT_MAX;

	v3f line01 = triangle[1] - triangle[0];
	v3f line12 = triangle[2] - triangle[1];
	v3f line20 = triangle[0] - triangle[2];
					
	Plane triPlane = {};
	triPlane.normal = cross(line01, line12);
	triPlane.distance = -dot(triPlane.normal, triangle[0]);

	f32 tNear = intersectPlane(&triPlane, rayOrigin, rayDirection);
	if (isValidIntersection(tNear))
	{
		f32 triangleArea = magnitude(cross(triangle[1] - triangle[0], triangle[2] - triangle[0]));
		v3f rayPlanePoint = rayOrigin + tNear*rayDirection;

		v3f rayTest = cross(line01, rayPlanePoint - triangle[0]);
		if (barycentric)
			barycentric->z = magnitude(rayTest) / triangleArea;
		
		if (dot(rayTest, triPlane.normal) < 0.0f)
			return t;
		
		rayTest = cross(line12, rayPlanePoint - triangle[1]);
		if (barycentric)
			barycentric->x = magnitude(rayTest) / triangleArea;
						
		if (dot(rayTest, triPlane.normal) < 0.0f)
			return t;

		rayTest = cross(line20, rayPlanePoint - triangle[2]);
		if (barycentric)
			barycentric->y = magnitude(rayTest) / triangleArea;

		if (dot(rayTest, triPlane.normal) < 0.0f)
		    return t;

		t = tNear;
	}

	return t;
}

// LIGHTING FUNCTIONS
static f32 castShadowRay(Scene* scene, v3f rayOrigin, v3f rayDirection)
{
	f32 tValue = FLT_MIN;

	Plane* plane = scene->planes;
	while (plane)
	{
		f32 tNear = intersectPlane(plane, rayOrigin, rayDirection);

		if (isValidIntersection(tNear))
		{
			tValue = tNear;
			break;
		}
			
		plane = plane->next;
	}

	Box* box = scene->boxes;
	while(!isValidIntersection(tValue) && box)
	{
		f32 tNear = intersectBox(box, rayOrigin, rayDirection, NULL);

		if (isValidIntersection(tNear))
		{
			tValue = tNear;
			break;
		}
		
		box = box->next;
	}

	Sphere* sphere = scene->spheres;
	while (!isValidIntersection(tValue) && sphere)
	{
		f32 tNear = intersectSphere(sphere, rayOrigin, rayDirection);

		if (isValidIntersection(tNear))
		{
			tValue = tNear;
			break;
		}
		
		sphere = sphere->next;
	}

	Mesh* mesh = scene->meshes;
	while (!isValidIntersection(tValue) && mesh)
	{
		for (u32 faceIndex = 0; faceIndex < mesh->numFaces; ++faceIndex)
		{
			v3* face = mesh->faces + faceIndex;
			if (!isValidFace(face, mesh->numVertices))
				continue;

			Triangle triangle = getTriangleFromFace(face, mesh->vertices);
			f32 tNear = intersectTriangle(triangle, rayOrigin, rayDirection, 0);

			if (isValidIntersection(tNear))
			{
				tValue = tNear;
				break;
			}
		}
		
		mesh = mesh->next;
	}
	
	return tValue;
}

static v3f calculateColour(Scene* scene, v3f viewPoint, v3f rayPoint, v3f objectNormal, Material* material, Lights lights)
{
	v3f colour;

	v3f ambientColour = {};
	v3f diffuseColour = {};
	v3f specularColour = {};

	v3f viewDirection = normalize(viewPoint - rayPoint);
	
	f32 shadowBias = 0.0001f;
	
	AmbientLight* ambient = lights.ambient;
	while (ambient)
	{
		ambientColour += hadamard(material->ambient, ambient->colour);
		ambient = ambient->next;
	}

	PointLight* point = lights.point;
	while (point)
	{
		v3f lightDirection = normalize(point->pos - rayPoint);

		v3f shadowRayOrigin = rayPoint + shadowBias*objectNormal;
		v3f shadowRayDirection = lightDirection;
		f32 tNear = castShadowRay(scene, shadowRayOrigin, shadowRayDirection);

		// this means that there is an object block this point's view of the light
		if (isValidIntersection(tNear))
		{
			point = point->next;
			continue;
		}
		
		f32 lightIntensity = dot(objectNormal, lightDirection);

		v3f reflected = normalize(2*lightIntensity*objectNormal - lightDirection);
		f32 specular = dot(viewDirection, reflected);

		if (specular < 0.0f)
			specular = 0.0f;
		else
			specular = pow(specular, material->shininess);
		
		if (lightIntensity < 0.0f)
			lightIntensity = 0.0f;

	    diffuseColour += hadamard(material->diffuse, point->colour) * lightIntensity;
		specularColour += hadamard(material->specular, point->colour) * specular;
		point = point->next;
	}

	DirectionalLight* directional = lights.directional;
	while (directional)
	{
		v3f lightDirection = -directional->direction;
		
		v3f shadowRayOrigin = rayPoint + shadowBias*objectNormal;
		v3f shadowRayDirection = lightDirection;
		f32 tNear = castShadowRay(scene, shadowRayOrigin, shadowRayDirection);

		if (isValidIntersection(tNear))
		{
			directional = directional->next;
			continue;
		}
		
		f32 lightIntensity = dot(objectNormal, lightDirection);

		v3f reflected = normalize(2*lightIntensity*objectNormal - lightDirection);
		f32 specular = dot(viewDirection, reflected);

		if (specular < 0.0f)
			specular = 0.0f;
		else
			specular = pow(specular, material->shininess);
		
		if (lightIntensity < 0.0f)
			lightIntensity = 0.0f;
		
		diffuseColour += hadamard(material->diffuse, directional->colour) * lightIntensity;
		specularColour += hadamard(material->specular, directional->colour) * specular;
		directional = directional->next;
	}

	colour = ambientColour + diffuseColour + specularColour;
	
	if (colour.r > 1.0f)
		colour.r = 1.0f;
	if (colour.g > 1.0f)
		colour.g = 1.0f;
	if (colour.b > 1.0f)
		colour.b = 1.0f;

	return colour;
}

void raytracer(void* data)
{
	RaytracerSettings* settings = (RaytracerSettings*)data;
	Bitmap* workBuffer = &settings->workBuffer;
	Camera* camera = &settings->scene.camera;
	Scene* scene = &settings->scene;

	v3f cameraY = camera->updir;
	v3f cameraX = normalize(cross(camera->viewDirection, cameraY));
	v3f cameraZ = normalize(cross(cameraX, cameraY));

	v3f imageCentre = camera->pos - cameraZ*camera->imagePlaneDistance;

	f32 imageAspectRatio = (f32)workBuffer->width / (f32)workBuffer->height;
	f32 imageHalfHeight = 1.0f;
	f32 imageHalfWidth = 1.0f;
	
	for (u32 y = 0; y < workBuffer->height; ++y)
	{
		// negate the entire thing because our bitmap's data is top-down
		f32 imageYRatio = -(2.0f*((f32)y / (f32)workBuffer->height) - 1.0f)*tan(camera->fov/2);
		
		for (u32 x = 0; x < workBuffer->width; ++x)
		{
			f32 imageXRatio = (2.0f*((f32)x / (f32)workBuffer->width) - 1.0f)*imageAspectRatio*tan(camera->fov/2);

			v3f imagePos =
			    imageCentre + imageXRatio*imageHalfWidth*cameraX + imageYRatio*imageHalfHeight*cameraY;

			v3f rayOrigin = camera->pos;
			v3f rayDirection = normalize(imagePos - rayOrigin);
			
			f32 intersectPoint = FLT_MAX;
			u32 colour = 0xFF9bdadd;
			
			//NOTE(denis): plane intersection test!
			Plane* plane = scene->planes;
			while (plane)
			{
				f32 tNear = intersectPlane(plane, rayOrigin, rayDirection);
				
				if (isValidIntersection(tNear) && tNear < intersectPoint)
				{
					intersectPoint = tNear;
					v3f rayPoint = rayOrigin + tNear*rayDirection;
					
					v3f planeColour = calculateColour(scene, rayOrigin, rayPoint, plane->normal, &plane->material, scene->lights);
					colour = convertColour(planeColour);
				}

				plane = plane->next;
			}

			Box* box = scene->boxes;
		    while (box)
			{
				v3f normal = {};
				f32 tNear = intersectBox(box, rayOrigin, rayDirection, &normal);
				
				if (isValidIntersection(tNear) && tNear < intersectPoint)
				{
					intersectPoint = tNear;

					v3f rayPoint = rayOrigin + tNear*rayDirection;
					
					v3f boxColour = calculateColour(scene, rayOrigin, rayPoint, normal, &box->material, scene->lights);
					colour = convertColour(boxColour);	
				}

				box = box->next;
			}
			
			//NOTE(denis): sphere intersection!
			Sphere* sphere = scene->spheres;
			while (sphere)
			{
				f32 tNear = intersectSphere(sphere, rayOrigin, rayDirection);
				
				if (isValidIntersection(tNear) && tNear < intersectPoint)
				{
					intersectPoint = tNear;

					v3f rayPoint = rayOrigin + tNear*rayDirection;
					v3f sphereNormal = normalize(rayPoint - sphere->pos);
						
					v3f sphereColour = calculateColour(scene, rayOrigin, rayPoint, sphereNormal, &sphere->material, scene->lights); 
					colour = convertColour(sphereColour);
				}

				sphere = sphere->next;
			}

			//NOTE(denis): mesh interesction
			Mesh* mesh = scene->meshes;
			while (mesh)
			{
			    for (u32 i = 0; i < mesh->numFaces; ++i)
				{
					v3* face = &mesh->faces[i];
					if (!isValidFace(face, mesh->numVertices))
						continue;

					Triangle triangle = getTriangleFromFace(face, mesh->vertices);
					v3f barycentric = {};
					f32 tNear = intersectTriangle(triangle, rayOrigin, rayDirection, &barycentric);

					if (isValidIntersection(tNear) && tNear < intersectPoint)
					{							
						intersectPoint = tNear;

						v3f triangleNormal = cross(triangle[1] - triangle[0], triangle[2] - triangle[1]);
						
						v3f normal;
						Material material = mesh->materials[0];
							
						if (mesh->normals && mesh->numNormals == mesh->numVertices)
						{
							v3f normal1 = mesh->normals[face->x];
							v3f normal2 = mesh->normals[face->y];
							v3f normal3 = mesh->normals[face->z];

							normal = interpolate(normal1, normal2, normal3, barycentric);
						}
						else
							normal = normalize(triangleNormal);

						if (mesh->materials && mesh->numMaterials == mesh->numVertices)
						{
							Material m1 = mesh->materials[face->x];
							Material m2 = mesh->materials[face->y];
							Material m3 = mesh->materials[face->z];

							material.ambient = interpolate(m1.ambient, m2.ambient, m3.ambient, barycentric);
							material.diffuse = interpolate(m1.diffuse, m2.diffuse, m3.diffuse, barycentric);
							material.specular = interpolate(m1.specular, m2.specular, m3.specular, barycentric);
							material.shininess = m1.shininess*barycentric.x + m2.shininess*barycentric.y + m3.shininess*barycentric.z;
						}

						v3f rayPoint = rayOrigin + tNear*rayDirection;
						
						v3f triangleColour = calculateColour(scene, rayOrigin, rayPoint, normal, &material, scene->lights);
						colour = convertColour(triangleColour);
					}
				}
				
				mesh = mesh->next;
			}

			drawPoint(workBuffer, x, y, colour);
		}
	}
	
	// ray tracing is complete so we update the result buffer and let the main thread know
	Bitmap* resultBuffer = &settings->resultBuffer;
	
	settings->platform.acquireLock(&settings->bufferLock);
	
	resultBuffer->width = workBuffer->width;
	resultBuffer->height = workBuffer->height;
	drawBitmap(resultBuffer, workBuffer, V2(0, 0));

	settings->platform.releaseLock(&settings->bufferLock);
	
	settings->raytracerComplete = true;
}
