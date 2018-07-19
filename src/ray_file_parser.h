#if !defined(RAY_FILE_PARSER_H_)
#define RAY_FILE_PARSER_H_

#include <stdio.h>
#include <vector>
#include "denis_strings.h"
#include "scene.h"

#define ADD_TO_SCENE_LIST(sceneList, object, type)			\
	do {													\
		type* newObject = (type*)HEAP_ALLOC(sizeof(type));	\
		*newObject = object;								\
		type* oldObject = sceneList;						\
		newObject->next = oldObject;						\
		sceneList = newObject;								\
	} while(0)

#define BUFFER_SIZE 200
struct FileData
{
	FILE* file;
	char* buffer;
};

static inline char* ray_getLine(FileData* fileData)
{
	char* result = 0;
    if (fgets(fileData->buffer, BUFFER_SIZE, fileData->file))
		result = trimString(fileData->buffer);

	return result;
}

//NOTE(denis): will only read the first number it finds in a line as a f32 value, ignores anything after
static f32 ray_readF32InLine(char* line)
{
	f32 result = 0.0f;

	if (!line)
		return result;

	bool numFound = false;
	char* numStartString = line;
	for (u32 i = 0; line[i] != 0 && !numFound; ++i)
	{
		if (IS_NUMBER(line[i]) || (line[i] == '-' && IS_NUMBER(line[i+1])))
		{
			numFound = true;
			numStartString = line + i;
		}
	}

	if (numFound)
	{
		result = parseF32String(numStartString);
	}

	return result;
}

static v3f ray_parseV3f(char* line)
{
	v3f result = {};

	bool vectorStarted = false;
	s32 currentValue = -1;
	bool readingValue = false;

	for (u32 i = 0; line[i] != 0; ++i)
	{
		if (!vectorStarted)
		{
			if (line[i] == '(')
				vectorStarted = true;
		}
		else
		{
			if (IS_NUMBER(line[i]) || (line[i] == '-' && IS_NUMBER(line[i+1])))
			{
				if (!readingValue)
				{
					readingValue = true;
					++currentValue;
				}
								
				u32 numLength;
				for (numLength = 0;
					 line[i+numLength] != ',' && line[i+numLength] != ' ' && line[i+numLength] != ')';
					 ++numLength)
					;

				char* numString = line + i;
				*(numString + numLength) = 0;
			    result[currentValue] = parseF32String(numString);

				i = i + numLength;
				readingValue = false;
				continue;
			}
			else if (line[i] == ')' || currentValue >= 2)
			{
				break;
			}
		}
	}

	return result;
}

static v3 ray_parseV3(char* line)
{
	v3 result = {};

	bool vectorStarted = false;
	s32 currentValue = -1;
	bool readingValue = false;

	for (u32 i = 0; line[i] != 0; ++i)
	{
		if (!vectorStarted)
		{
			if (line[i] == '(')
				vectorStarted = true;
		}
		else
		{
			if (IS_NUMBER(line[i]) || (line[i] == '-' && IS_NUMBER(line[i+1])))
			{
				if (!readingValue)
				{
					readingValue = true;
					++currentValue;
				}
								
				u32 numLength;
				for (numLength = 0;
					 line[i+numLength] != ',' && line[i+numLength] != ' ' && line[i+numLength] != ')';
					 ++numLength)
					;

				char* numString = line + i;
				*(numString + numLength) = 0;
			    result[currentValue] = parseS32String(numString);

				i = i + numLength;
				readingValue = false;
				continue;
			}
			else if (line[i] == ')' || currentValue >= 2)
			{
				break;
			}
		}
	}

	return result;
}

enum
{
	CAMERA,
	PLANE,
	SPHERE,
	BOX,
	MESH,
	MATERIAL,
	AMBIENT_LIGHT,
	POINT_LIGHT,
	DIRECTIONAL_LIGHT,

	NUM_TYPES
};

struct ParseResult
{
	u32 type;
	union
	{
		Camera camera;
		Plane plane;
		Sphere sphere;
		Box box;
		Mesh mesh;
		Material material;
		AmbientLight ambientLight;
		PointLight pointLight;
		DirectionalLight directionalLight;
	};
};

static ParseResult ray_parseObject(FileData* fileData, char* startLine);

static ParseResult ray_parseObjectMaterial(FileData* fileData, char* startLine)
{
	ParseResult result = {};
	result.type = MATERIAL;
	// default material properties
	result.material = getMaterial();

	if (stringStartsWith(startLine, (char*)"material"))
	{
		result = ray_parseObject(fileData, startLine);
		return result;
	}
	
	if (!charInString('{', startLine))
	{
		//TODO(denis): check the next line for the opening brace?
		return result;
	}

	char* line;
	while ((line = ray_getLine(fileData)))
	{
		if (line[0] == '}')
		{
			break;
		}
		else
		{
		    result = ray_parseObject(fileData, line);
		}
	}
	
	return result;
}

static ParseResult ray_parseObject(FileData* fileData, char* startLine)
{
	ParseResult result = {};
	result.type = NUM_TYPES;
	
	char* line = startLine;
	if (!line)
		return result;
	
	if (stringStartsWith(startLine, (char*)"camera"))
	{
		result.type = CAMERA;

		result.camera.viewDirection = V3f(0.0f, 0.0f, -1.0f);
		result.camera.imagePlaneDistance = 1.0f;
		result.camera.fov = (f32)M_PI / 2.0f; // default is 90 degrees
		result.camera.updir = V3f(0.0f, 1.0f, 0.0f);

		while ((line = ray_getLine(fileData)))
		{
			if (stringStartsWith(line, (char*)"position"))
			{
			    result.camera.pos = ray_parseV3f(line);
			}
			else if (stringStartsWith(line, (char*)"viewdir"))
			{
				result.camera.viewDirection = normalize(ray_parseV3f(line));
			}
			else if (stringStartsWith(line, (char*)"fov"))
			{
				f32 fovDegrees = ray_readF32InLine(line);
				result.camera.fov = fovDegrees * ((f32)M_PI/180.0f);
			}
			else if (stringStartsWith(line, (char*)"updir"))
			{
				result.camera.updir = normalize(ray_parseV3f(line));
			}
			else if (line[0] == '}')
			{
				break;
			}
		}
	}
	else if (stringStartsWith(startLine, (char*)"ambient_light"))
	{
		result.type = AMBIENT_LIGHT;

		while ((line = ray_getLine(fileData)))
		{
		    if (stringStartsWith(line, (char*)"colour") || stringStartsWith(line, (char*)"color"))
			{
				result.ambientLight.colour = ray_parseV3f(line);
			}
			else if (line[0] == '}')
				break;
		}
	}
	else if (stringStartsWith(startLine, (char*)"point_light"))
	{
		result.type = POINT_LIGHT;
		
		while ((line = ray_getLine(fileData)))
		{
			if (stringStartsWith(line, (char*)"position"))
			{
				result.pointLight.pos = ray_parseV3f(line);
			}
			else if (stringStartsWith(line, (char*)"colour") || stringStartsWith(line, (char*)"color"))
			{
				result.pointLight.colour = ray_parseV3f(line);
			}
			else if (stringStartsWith(line, (char*)"constant_attenuation_coeff"))
			{
				result.pointLight.constantAttenuation = ray_readF32InLine(line);
			}
			else if (stringStartsWith(line, (char*)"linear_attenuation_coeff"))
			{
				result.pointLight.linearAttenuation = ray_readF32InLine(line);
			}
			else if (stringStartsWith(line, (char*)"quadratic_attenuation_coeff"))
			{
				result.pointLight.quadraticAttenuation = ray_readF32InLine(line);
			}
			else if (line[0] == '}')
			{
				break;
			}
		}
	}
	else if (stringStartsWith(startLine, (char*)"directional_light"))
	{
		result.type = DIRECTIONAL_LIGHT;
		
		while((line = ray_getLine(fileData)))
		{
			if (stringStartsWith(line, (char*)"direction"))
			{
				result.directionalLight.direction = normalize(ray_parseV3f(line));
			}
			else if (stringStartsWith(line, (char*)"colour") || stringStartsWith(line, (char*)"color"))
			{
				result.directionalLight.colour = ray_parseV3f(line);
			}
			else if (line[0] == '}')
				break;
		}
	}
	else if (stringStartsWith(startLine, (char*)"square"))
	{
		result.type = PLANE;
		result.plane.normal = V3f(0.0f, 1.0f, 0.0f);

		ParseResult materialResult = ray_parseObjectMaterial(fileData, line);
		result.plane.material = materialResult.material;
	}
	else if (stringStartsWith(startLine, (char*)"box"))
	{
		result.type = BOX;
		result.box.min = V3f(-0.5f, -0.5f, -0.5f);
		result.box.max = V3f(0.5f, 0.5f, 0.5f);

		ParseResult materialResult = ray_parseObjectMaterial(fileData, line);
	    result.box.material = materialResult.material;
	}
	else if (stringStartsWith(startLine, (char*)"sphere"))
	{
		result.type = SPHERE;
	    result.sphere.radius = 1.0f;

		ParseResult materialResult = ray_parseObjectMaterial(fileData, line);
		result.sphere.material = materialResult.material;
	}
	else if (stringStartsWith(startLine, (char*)"polymesh") || stringStartsWith(startLine, (char*)"trimesh"))
	{
		std::vector<v3f> points;
		std::vector<v3> faces;
		std::vector<v3f> normals;
		std::vector<Material> materials;
		
		bool readingPoints = false;
		bool readingFaces = false;
		bool readingNormals = false;
		while (true)
		{
			line = ray_getLine(fileData);
			if (!line)
				continue;
			else if (line[0] == '}')
				break;
			
			if (readingPoints)
			{
				bool isPoint = false;
				for (u32 i = 0; line[i] != 0; ++i)
				{
					if (line[i] == '(')
						isPoint = true;

					if (isPoint && line[i] == ')')
					{
						isPoint = false;
						v3f point = ray_parseV3f(line);
						points.push_back(point);
					}
					else if (line[i] == ')')
						readingPoints = false;
				}
			}
			else if (readingFaces)
			{
				bool isFace = false;
				for (u32 i = 0; line[i] != 0; ++i)
				{
					if (line[i] == '(')
					    isFace = true;

					if (isFace && line[i] == ')')
					{
					    isFace = false;
						v3 face = ray_parseV3(line);
						faces.push_back(face);
					}
					else if (line[i] == ')')
					    readingFaces = false;
				}
			}
			else if (readingNormals)
			{
				bool isNormal = false;
				for (u32 i = 0; line[i] != 0; ++i)
				{
					if (line[i] == '(')
						isNormal = true;

					if (isNormal && line[i] == ')')
					{
						isNormal = false;
						v3f normal = ray_parseV3f(line);
						normals.push_back(normal);
					}
					else if (line[i] == ')')
						readingNormals = false;
				}				
			}
			
			if (stringStartsWith(line, (char*)"points"))
			{
				readingPoints = true;
			}
			else if (stringStartsWith(line, (char*)"faces"))
			{
				readingFaces = true;
			}
			else if (stringStartsWith(line, (char*)"normals"))
			{
				readingNormals = true;
			}
			else if (stringStartsWith(line, (char*)"materials"))
			{
				bool materialStarted = false;
				while ((line = ray_getLine(fileData)))
				{
				    if (line[0] == '{')
					{
						ParseResult materialResult = ray_parseObject(fileData, (char*)"material");
						materials.push_back(materialResult.material);
						
						// we are now on the line with a '}' after the material data
						// start looking after the '}' character for the ending ')'
						bool materialsEnd = false;
						for (u32 i = 1; line[i] != 0 && !materialsEnd; ++i)
						{
							if (line[i] == ')')
								materialsEnd = true;
						}

						if (materialsEnd)
							break;
					}
				}
			}
			else if (stringStartsWith(line, (char*)"material"))
			{
				ParseResult materialResult = ray_parseObjectMaterial(fileData, line);
				materials.push_back(materialResult.material);
			}
		}

		result.type = MESH;
		
		result.mesh.numVertices = (u32)points.size();
		result.mesh.vertices = (v3f*)HEAP_ALLOC(sizeof(v3f)*result.mesh.numVertices);
		for (u32 i = 0; i < points.size(); ++i)
			result.mesh.vertices[i] = points[i];

		result.mesh.numFaces = (u32)faces.size();
		result.mesh.faces = (v3*)HEAP_ALLOC(sizeof(v3)*result.mesh.numFaces);
		for (u32 i = 0; i < faces.size(); ++i)
			result.mesh.faces[i] = faces[i];

		result.mesh.numNormals = (u32)normals.size();
		result.mesh.normals = (v3f*)HEAP_ALLOC(sizeof(v3f)*result.mesh.numNormals);
		for (u32 i = 0; i < normals.size(); ++i)
			result.mesh.normals[i] = normals[i];

		result.mesh.numMaterials = (u32)materials.size();
		result.mesh.materials = (Material*)HEAP_ALLOC(sizeof(Material)*result.mesh.numMaterials);
		for (u32 i = 0; i < materials.size(); ++i)
			result.mesh.materials[i] = materials[i];
	}
	else if (stringStartsWith(startLine, (char*)"material"))
	{
		result.type = MATERIAL;

		Material material = {};
		material.ambient = V3f(1.0f, 1.0f, 1.0f);
		material.shininess = 1.0f;

		while ((line = ray_getLine(fileData)))
		{
			if (line[0] == '}')
			{
			    result.material = material;
				break;
			}
			else if (stringStartsWith(line, (char*)"ambient"))
			{
				material.ambient = ray_parseV3f(line);
			}
			else if (stringStartsWith(line, (char*)"diffuse"))
			{
				material.diffuse = ray_parseV3f(line);
			}
			else if (stringStartsWith(line, (char*)"specular"))
			{
				material.specular = ray_parseV3f(line);
			}
			else if (stringStartsWith(line, (char*)"shininess"))
			{
				material.shininess = ray_readF32InLine(line);
			}
		}
	}
	else if (stringStartsWith(startLine, (char*)"translate"))
	{
	    v3f translation = ray_parseV3f(line);

		// assumes the next property after translate is on a new line
		line = ray_getLine(fileData);
		result = ray_parseObject(fileData, line);

		if (result.type == SPHERE)
		{
			result.sphere.pos += translation;
		}
		else if (result.type == PLANE)
		{
			// so that adding translation is intuitive, positive means further along the normal
			result.plane.distance = -translation.y;
		}
		else if (result.type == BOX)
		{
			result.box.min -= translation;
			result.box.max -= translation;
		}
		else if (result.type == MESH)
		{
			for (u32 i = 0; i < result.mesh.numVertices; ++i)
			{
				result.mesh.vertices[i] += translation;
			}
		}
	}
	else if (stringStartsWith(startLine, (char*)"scale"))
	{
		bool scaleIsVector = false;
		bool foundFirstNum = false;
		for (u32 i = 0; line[i] != 0 && line[i] != '\n'; ++i)
		{
			if (!foundFirstNum && IS_NUMBER(line[i]))
			{
				foundFirstNum = true;
				while (IS_NUMBER(line[i+1]))
					++i;
			}
			else if (foundFirstNum)
			{
				if (IS_NUMBER(line[i]))
				{
					scaleIsVector = true;
					break;
				}
			}	
		}

		v3f scale = {};
		if (scaleIsVector)
		{
			scale = ray_parseV3f(line);
		}
		else
		{
			f32 scaleValue = ray_readF32InLine(line);
			scale = V3f(scaleValue, scaleValue, scaleValue);
		}

		// assumes the next property after scale is on a new line
		line = ray_getLine(fileData);
	    result = ray_parseObject(fileData, line);

		if (result.type == SPHERE)
		{
			// spheres should always have scale values that are all equal
			result.sphere.radius *= scale.x;
		}
		else if (result.type == BOX)
		{
			result.box.min = hadamard(result.box.min, scale);
			result.box.max = hadamard(result.box.max, scale);
		}
		else if (result.type == MESH)
		{
			for (u32 i = 0; i < result.mesh.numVertices; ++i)
			{
				v3f* vertex = result.mesh.vertices + i;
				*vertex = hadamard(*vertex, scale);
			}
		}
	}

	return result;
}

//NOTE: this is not a very great or robust parser! I built it so that it works with the files included in the data
// directory. It doesn't do all the syntax parsing that a proper parser should.
static Scene parseRayFile(char* rayFileName)
{
	Scene scene = {};

	FILE* rayFile = fopen(rayFileName, "r");
	if (!rayFile)
		return scene;

	char lineBuffer[BUFFER_SIZE];

	FileData fileData = {};
	fileData.file = rayFile;
	fileData.buffer = lineBuffer;
	
	fseek(rayFile, 0, SEEK_SET);
	while (fgets(lineBuffer, BUFFER_SIZE, rayFile))
	{
		if (lineBuffer[0] == '\n')
			continue;
		
		char* line = trimString(lineBuffer);
		if (!line)
			continue;

		if (IS_LETTER(line[0]))
		{
			ParseResult object = ray_parseObject(&fileData, line);

			if (object.type == NUM_TYPES)
				continue;

			if (object.type == SPHERE)
			{
				ADD_TO_SCENE_LIST(scene.spheres, object.sphere, Sphere);
			}
			else if (object.type == BOX)
			{
				ADD_TO_SCENE_LIST(scene.boxes, object.box, Box);
			}
			else if (object.type == PLANE)
			{
				ADD_TO_SCENE_LIST(scene.planes, object.plane, Plane);
			}
			else if (object.type == MESH)
			{
				ADD_TO_SCENE_LIST(scene.meshes, object.mesh, Mesh);
			}
			else if (object.type == CAMERA)
			{
				scene.camera = object.camera;
			}
			else if (object.type == AMBIENT_LIGHT)
			{
				ADD_TO_SCENE_LIST(scene.lights.ambient, object.ambientLight, AmbientLight);
			}
			else if (object.type == POINT_LIGHT)
			{
				ADD_TO_SCENE_LIST(scene.lights.point, object.pointLight, PointLight);
			}
			else if (object.type == DIRECTIONAL_LIGHT)
			{
				ADD_TO_SCENE_LIST(scene.lights.directional, object.directionalLight, DirectionalLight);
			}
		}
	}
	
	return scene;
}

#endif
