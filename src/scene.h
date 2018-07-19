#if !defined(SCENE_H_)
#define SCENE_H_

struct Camera
{
    v3f pos;
    v3f viewDirection;
	v3f updir;
	
	f32 imagePlaneDistance;
	
    f32 fov; // in radians
};

struct AmbientLight
{
	v3f colour;

	AmbientLight* next;
};

struct PointLight
{
	v3f pos;
	v3f colour;

	//TODO(denis): these are all for distance attenuation
	f32 constantAttenuation;
	f32 linearAttenuation;
	f32 quadraticAttenuation;

	PointLight* next;
};

struct DirectionalLight
{
	v3f direction;
	v3f colour;

	DirectionalLight* next;
};

struct Lights
{
	AmbientLight* ambient;
	PointLight* point;
	DirectionalLight* directional;
};

struct Material
{
	v3f ambient;
	v3f diffuse;
	v3f specular;

	f32 shininess;
};

Material getMaterial()
{
	Material result = {};
	result.ambient = V3f(0.3f, 0.3f, 0.3f);
	result.diffuse = V3f(0.5f, 0.5f, 0.5f);
	result.specular = V3f(0.0f, 0.5f, 0.0f);
	result.shininess = 24.0f;
	return result;
}

struct Sphere
{
	v3f pos;
	f32 radius;

	Material material;

	Sphere* next;
};

struct Box
{
	v3f min;
	v3f max;

	Material material;

	Box* next;
};

struct Plane
{
	v3f normal;
	f32 distance;

	Material material;

	Plane* next;
};

struct Mesh
{
	u32 numVertices;
	v3f* vertices;

	//NOTE(denis): the number of normals should either be 0 or the same as numVertices
	u32 numNormals;
	v3f* normals;
	
	//NOTE(denis): faces are defined in a counter-clockwise order
	u32 numFaces;
	v3* faces;

	//NOTE(denis): should either be one or the same as numVertices
	u32 numMaterials;
	Material* materials;

	Mesh* next;
};

struct Scene
{
	Camera camera;

	Lights lights;
	
	Plane* planes;
	Box* boxes;
	Sphere* spheres;
	Mesh* meshes;
};

#endif
