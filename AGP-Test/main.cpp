// MD2 animation renderer
// This demo will load and render an animated MD2 model, an OBJ model and a skybox
// Most of the OpenGL code for dealing with buffer objects, etc has been moved to a 
// utility library, to make creation and display of mesh objects as simple as possible


// Windows specific: Uncomment the following line to open a console window for debug output
#if _DEBUG
#pragma comment(linker, "/subsystem:\"console\" /entry:\"WinMainCRTStartup\"")
#endif

#include "rt3d.h"
#include "rt3dObjLoader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stack>

using namespace std;

#define DEG_TO_RADIAN 0.017453293

// Globals are being used (for now...)

GLuint cubeIndexCount = 0;
GLuint bunnyIndexCount = 0;
GLuint meshObjects[3];

// Rotates the Camera
GLfloat r = 0.0f;

// Vectors applied to camera
glm::vec3 eye(-2.0f, 1.0f, 8.0f);
glm::vec3 at(0.0f, 1.0f, -1.0f);
glm::vec3 up(0.0f, 1.0f, 0.0f);

// Shader programs (in the same order of the specification)

// Gouraud
GLuint gouraudProgram;

// Phong
GLuint actualPhongProgram;

// Refraction
GLuint refractionProgram;

// Environment mapping (reflection)
GLuint EnviroMapProgram;

// Cartoon
GLuint toonProgram;

// Skybox
GLuint skyboxProgram;

GLuint textureProgram;
GLuint shaderProgram;

stack<glm::mat4> mvStack;

GLuint uniformIndex;

// TEXTURE STUFF
GLuint textures[3];
GLuint skybox[5];

rt3d::lightStruct light0 = {
	{ 0.4f, 0.4f, 0.4f, 1.0f }, // ambient
	{ 1.0f, 1.0f, 1.0f, 1.0f }, // diffuse
	{ 1.0f, 1.0f, 1.0f, 1.0f }, // specular
	{ -5.0f, 2.0f, 2.0f, 1.0f }  // position
};
glm::vec4 lightPos(-5.0f, 2.0f, 2.0f, 1.0f); //light position

rt3d::materialStruct material0 = {
	{ 0.2f, 0.4f, 0.2f, 1.0f }, // ambient
	{ 0.5f, 1.0f, 0.5f, 1.0f }, // diffuse
	{ 0.0f, 0.1f, 0.0f, 1.0f }, // specular
	2.0f  // shininess
};
rt3d::materialStruct material1 = {
	{ 0.4f, 0.4f, 1.0f, 1.0f }, // ambient
	{ 0.8f, 0.8f, 1.0f, 1.0f }, // diffuse
	{ 0.8f, 0.8f, 0.8f, 1.0f }, // specular
	1.0f  // shininess
};

// For cycling through the shaders
int shaderController = 1;

// Light attenuation (Taken from Lab4 base code)
float attConstant = 1.0f;
float attLinear = 0.0f;
float attQuadratic = 0.0f;

// Set up rendering context using SDL
SDL_Window * setupRC(SDL_GLContext &context) {
	SDL_Window * window;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) // Initialize video
		rt3d::exitFatalError("Unable to initialize SDL");

	// Request an OpenGL 3.0 context.

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);  // double buffering on
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8); // 8 bit alpha buffering
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // Turn on x4 multisampling anti-aliasing (MSAA)

													   // Create 800x600 window
	window = SDL_CreateWindow("SDL/GLM/OpenGL Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!window) // Check window was created OK
		rt3d::exitFatalError("Unable to create window");

	context = SDL_GL_CreateContext(window); // Create opengl context and attach to window
	SDL_GL_SetSwapInterval(1); // set swap buffers to sync with monitor's vertical refresh rate
	return window;
}

// A simple texture loading function to load from bitmap files

GLuint loadBitmap(const char *fname) {
	GLuint texID;
	glGenTextures(1, &texID); // generate texture ID

							  // load file - using core SDL library
	SDL_Surface *tmpSurface;
	tmpSurface = SDL_LoadBMP(fname);
	if (!tmpSurface) {
		std::cout << "Error loading bitmap" << std::endl;
	}

	// bind texture and set parameters
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SDL_PixelFormat *format = tmpSurface->format;

	GLuint externalFormat, internalFormat;
	if (format->Amask) {
		internalFormat = GL_RGBA;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGBA : GL_BGRA;
	}
	else {
		internalFormat = GL_RGB;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGB : GL_BGR;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, tmpSurface->w, tmpSurface->h, 0,
		externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);

	SDL_FreeSurface(tmpSurface); // texture loaded, free the temporary buffer
	return texID;	// return value of texture ID
}


// A simple cubemap loading function
GLuint loadCubeMap(const char *fname[6], GLuint *texID)
{
	glGenTextures(1, texID); // generate texture ID
	GLenum sides[6] = { GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y };
	SDL_Surface *tmpSurface;

	glBindTexture(GL_TEXTURE_CUBE_MAP, *texID); // bind texture and set parameters
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	GLuint externalFormat;
	for (int i = 0; i<6; i++)
	{
		// load file - using core SDL library
		tmpSurface = SDL_LoadBMP(fname[i]);
		if (!tmpSurface)
		{
			std::cout << "Error loading bitmap" << std::endl;
			return *texID;
		}

		// skybox textures should not have alpha (assuming this is true!)
		SDL_PixelFormat *format = tmpSurface->format;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGB : GL_BGR;

		glTexImage2D(sides[i], 0, GL_RGB, tmpSurface->w, tmpSurface->h, 0,
			externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
		// texture loaded, free the temporary buffer
		SDL_FreeSurface(tmpSurface);
	}
	return *texID;	// return value of texure ID, redundant really
}


void init(void) {

	// Initialising shaders

	// For Gouraud
	gouraudProgram = rt3d::initShaders("gouraud.vert", "simple.frag");
	rt3d::setLight(gouraudProgram, light0);
	rt3d::setMaterial(gouraudProgram, material0);

	// For Phong
	actualPhongProgram = rt3d::initShaders("ActualPhong.vert", "ActualPhong.frag");
	rt3d::setLight(actualPhongProgram, light0);
	rt3d::setMaterial(actualPhongProgram, material0);

	// For Refraction
	refractionProgram = rt3d::initShaders("Refraction.vert", "Refraction.frag");
	rt3d::setLight(shaderProgram, light0);
	rt3d::setMaterial(shaderProgram, material0);

	// set light attenuation shader uniforms
	// Code below was taken from the Lab 4 base code during week 5

	uniformIndex = glGetUniformLocation(refractionProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(refractionProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(refractionProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);


	shaderProgram = rt3d::initShaders("EnviroMap.vert", "EnviroMap.frag");
	rt3d::setLight(shaderProgram, light0);
	rt3d::setMaterial(shaderProgram, material0);

	// set light attenuation shader uniforms
	// Code below was taken from the Lab 4 base code during week 5

	uniformIndex = glGetUniformLocation(shaderProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(shaderProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(shaderProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);

	// For Environment mapping (Reflection)
	EnviroMapProgram = rt3d::initShaders("EnviroMap.vert", "EnviroMap.frag");
	rt3d::setLight(EnviroMapProgram, light0);
	rt3d::setMaterial(EnviroMapProgram, material0);

	// set light attenuation shader uniforms
	// Code below was taken from the Lab 4 base code during week 5

	uniformIndex = glGetUniformLocation(EnviroMapProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(EnviroMapProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(EnviroMapProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);
	uniformIndex = glGetUniformLocation(EnviroMapProgram, "cameraPos");
	glUniform3fv(uniformIndex, 1, glm::value_ptr(eye));

	// For Cartoon (toon)
	toonProgram = rt3d::initShaders("toon.vert", "toon.frag");
	rt3d::setLight(toonProgram, light0);
	rt3d::setMaterial(toonProgram, material0);

	// set light attenuation shader uniforms
	// Code below was taken from the Lab 4 base code during week 5

	uniformIndex = glGetUniformLocation(toonProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(toonProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(toonProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);

	textureProgram = rt3d::initShaders("textured.vert", "textured.frag");
	
	// Cube mape shaders/texture for skybox
	skyboxProgram = rt3d::initShaders("cubeMap.vert", "cubeMap.frag");

	// 6 BMPs for Skybox, one BMP for each face of the cube
	// Again, taken from Lab 4 base code during week 5

	const char *cubeTexFiles[6] =

	{
		"Town-skybox/Town_bk.bmp", "Town-skybox/Town_ft.bmp", "Town-skybox/Town_rt.bmp", "Town-skybox/Town_lf.bmp", "Town-skybox/Town_up.bmp", "Town-skybox/Town_dn.bmp"
	};

	loadCubeMap(cubeTexFiles, &skybox[0]);


	vector<GLfloat> verts;
	vector<GLfloat> norms;
	vector<GLfloat> tex_coords;
	vector<GLuint> indices;
	rt3d::loadObj("cube.obj", verts, norms, tex_coords, indices);
	cubeIndexCount = indices.size();
	textures[0] = loadBitmap("fabric.bmp");
	meshObjects[0] = rt3d::createMesh(verts.size() / 3, verts.data(), nullptr, norms.data(), tex_coords.data(), cubeIndexCount, indices.data());

	textures[2] = loadBitmap("studdedmetal.bmp");


	verts.clear(); norms.clear(); tex_coords.clear(); indices.clear();
	rt3d::loadObj("bunny-5000.obj", verts, norms, tex_coords, indices);
	bunnyIndexCount = indices.size();
	meshObjects[2] = rt3d::createMesh(verts.size() / 3, verts.data(), nullptr, norms.data(), nullptr, bunnyIndexCount, indices.data());

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}

glm::vec3 moveForward(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d * std::sin(r*DEG_TO_RADIAN), pos.y, pos.z - d * std::cos(r*DEG_TO_RADIAN));
}

glm::vec3 moveRight(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d * std::cos(r*DEG_TO_RADIAN), pos.y, pos.z + d * std::sin(r*DEG_TO_RADIAN));
}

void update(void) {
	const Uint8 *keys = SDL_GetKeyboardState(NULL);

	// Controls for moving the camera:
	// Back and Forth 
	// Left and Right
	// Up and Down

	if (keys[SDL_SCANCODE_W]) eye = moveForward(eye, r, 0.1f);
	if (keys[SDL_SCANCODE_S]) eye = moveForward(eye, r, -0.1f);
	if (keys[SDL_SCANCODE_A]) eye = moveRight(eye, r, -0.1f);
	if (keys[SDL_SCANCODE_D]) eye = moveRight(eye, r, 0.1f);
	if (keys[SDL_SCANCODE_R]) eye.y += 0.1;
	if (keys[SDL_SCANCODE_F]) eye.y -= 0.1;

	// Controls for moving the light:
	// Taken from the base code of Lab 4 during Week 5

	if (keys[SDL_SCANCODE_I]) lightPos[2] -= 0.1;
	if (keys[SDL_SCANCODE_J]) lightPos[0] -= 0.1;
	if (keys[SDL_SCANCODE_K]) lightPos[2] += 0.1;
	if (keys[SDL_SCANCODE_L]) lightPos[0] += 0.1;
	if (keys[SDL_SCANCODE_U]) lightPos[1] += 0.1;
	if (keys[SDL_SCANCODE_H]) lightPos[1] -= 0.1;

	// Controls to cycle through the 5 shaders (In order of specification)

	if (keys[SDL_SCANCODE_1]) shaderController = 1; // Gouraud Shader
	if (keys[SDL_SCANCODE_2]) shaderController = 2; // Phong Shader
	if (keys[SDL_SCANCODE_3]) shaderController = 3; // Refraction Map Shader
	if (keys[SDL_SCANCODE_4]) shaderController = 4; // Environment Map Shader
	if (keys[SDL_SCANCODE_5]) shaderController = 5; // Cartoon Shader

}

// Draw function for Gouraud shader

void drawGouraud(glm::vec4 tmp, glm::mat4 projection)
{

	glUseProgram(gouraudProgram);

	rt3d::setLightPos(gouraudProgram, glm::value_ptr(tmp));
	rt3d::setUniformMatrix4fv(gouraudProgram, "projection", glm::value_ptr(projection));

	//set modelview
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0, 20.0, 20.0));
	rt3d::setUniformMatrix4fv(gouraudProgram, "modelview", glm::value_ptr(mvStack.top()));

	// Method to apply shader
	rt3d::setMaterial(gouraudProgram, material0);

	// Method to draw object
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);
	
	mvStack.pop();
}

// Draw function for Phong shader

void drawActPhong(glm::vec4 tmp, glm::mat4 projection)
{

	glUseProgram(actualPhongProgram);

	// Function for setting light on the shader
	rt3d::setLightPos(actualPhongProgram, glm::value_ptr(tmp));

	// Function for setting projection matrices to the shader
	rt3d::setUniformMatrix4fv(actualPhongProgram, "projection", glm::value_ptr(projection));

	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0, 20.0, 20.0));
	rt3d::setUniformMatrix4fv(actualPhongProgram, "modelview", glm::value_ptr(mvStack.top()));

	// Method to apply shader
	rt3d::setMaterial(actualPhongProgram, material0);

	// Method to draw object
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);
	
	mvStack.pop();

}

// Draw function for Environment mapping (Reflection)

void drawReflection(glm::vec4 tmp, glm::mat4 projection)
{

	glUseProgram(EnviroMapProgram);
	rt3d::setUniformMatrix4fv(EnviroMapProgram, "projection", glm::value_ptr(projection));

	// Function for setting light through the shader
	rt3d::setLightPos(EnviroMapProgram, glm::value_ptr(tmp));

	glBindTexture(GL_TEXTURE_2D, textures[2]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0f, 20.0f, 20.0f));
	rt3d::setUniformMatrix4fv(EnviroMapProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(EnviroMapProgram, material1);

	glm::mat4 modelMatrix(1.0);
	mvStack.push(mvStack.top());
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.0f, 1.0f, -3.0f));
	modelMatrix = glm::scale(modelMatrix, glm::vec3(20.0f, 20.0f, 20.0f));
	mvStack.top() = mvStack.top() * modelMatrix;
	
	// Method to apply shader
	rt3d::setUniformMatrix4fv(EnviroMapProgram, "modelMatrix", glm::value_ptr(mvStack.top()));

	// Method to draw object
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);

	mvStack.pop();
	mvStack.pop();
}

// Draw function for Refraction

void drawRefraction(glm::vec4 tmp, glm::mat4 projection)
{

	glUseProgram(refractionProgram);
	rt3d::setUniformMatrix4fv(refractionProgram, "projection", glm::value_ptr(projection));

	// Function for setting light through the shader
	rt3d::setLightPos(refractionProgram, glm::value_ptr(tmp));

	glBindTexture(GL_TEXTURE_2D, textures[2]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0f, 20.0f, 20.0f));
	rt3d::setUniformMatrix4fv(refractionProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(refractionProgram, material1);

	glm::mat4 modelMatrix(1.0);
	mvStack.push(mvStack.top());
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-2.0f, 1.0f, -3.0f));
	modelMatrix = glm::scale(modelMatrix, glm::vec3(20.0f, 20.0f, 20.0f));
	mvStack.top() = mvStack.top() * modelMatrix;

	// Method to apply shader
	rt3d::setUniformMatrix4fv(refractionProgram, "modelMatrix", glm::value_ptr(mvStack.top()));

	// Method to draw object
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);

	mvStack.pop();
	mvStack.pop();

}

// Draw function for Cartoon shading

void drawCartoon(glm::vec4 tmp, glm::mat4 projection)
{
	
	glUseProgram(toonProgram);

	// Function for setting light through the shader
	rt3d::setLightPos(toonProgram, glm::value_ptr(tmp));

	// Function for setting projection matrices to the shader
	rt3d::setUniformMatrix4fv(toonProgram, "projection", glm::value_ptr(projection));

	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-2.0f, 1.0f, -3.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0, 20.0, 20.0));
	rt3d::setUniformMatrix4fv(toonProgram, "modelview", glm::value_ptr(mvStack.top()));

	// Method to apply shader
	rt3d::setMaterial(toonProgram, material0);

	// Method to draw object
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);

	mvStack.pop();

}

void draw(SDL_Window * window) {
	
	// clear the screen
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection(1.0);
	projection = glm::perspective(float(60.0f*DEG_TO_RADIAN), 800.0f / 600.0f, 1.0f, 150.0f);

	glm::mat4 modelview(1.0); // set base position for scene
	mvStack.push(modelview);

	at = moveForward(eye, r, 1.0f);
	mvStack.top() = glm::lookAt(eye, at, up);

	// We will be using the cube map for the skybox
	glUseProgram(skyboxProgram);
	rt3d::setUniformMatrix4fv(skyboxProgram, "projection", glm::value_ptr(projection));

	glDepthMask(GL_FALSE); // make sure writing to update depth test is off
	glm::mat3 mvRotOnlyMat3 = glm::mat3(mvStack.top());
	mvStack.push(glm::mat4(mvRotOnlyMat3));

	glCullFace(GL_FRONT); // drawing inside of cube!
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox[0]);
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(1.5f, 1.5f, 1.5f));
	rt3d::setUniformMatrix4fv(skyboxProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::drawIndexedMesh(meshObjects[0], cubeIndexCount, GL_TRIANGLES);
	mvStack.pop();
	glCullFace(GL_BACK); // We're drawing inside the cube

	glDepthMask(GL_TRUE); // Make sure depth test is on

	glUseProgram(shaderProgram);
	rt3d::setUniformMatrix4fv(shaderProgram, "projection", glm::value_ptr(projection));

	glm::vec4 tmp = mvStack.top()*lightPos;
	light0.position[0] = tmp.x;
	light0.position[1] = tmp.y;
	light0.position[2] = tmp.z;
	rt3d::setLightPos(shaderProgram, glm::value_ptr(tmp));

	// Draws ground plane
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-10.0f, -0.1f, -10.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0f, 0.1f, 20.0f));
	rt3d::setUniformMatrix4fv(shaderProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(shaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[0], cubeIndexCount, GL_TRIANGLES);
	mvStack.pop();

	// This should draw the cube where the light is based
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(lightPos[0], lightPos[1], lightPos[2]));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(0.25f, 0.25f, 0.25f));
	rt3d::setUniformMatrix4fv(shaderProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(shaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[0], cubeIndexCount, GL_TRIANGLES);
	mvStack.pop();

	// User input detection - For cycling through the five shaders:
	// 1 = Gouraud
	// 2 = Phong
	// 3 = Refracted
	// 4 = Environment mapping
	// 5 = Car(toon)


	if (shaderController == 1) drawGouraud(tmp, projection);
	if (shaderController == 2) drawActPhong(tmp, projection);
	if (shaderController == 3) drawRefraction(tmp, projection);
	if (shaderController == 4) drawReflection(tmp, projection);
	if (shaderController == 5) drawCartoon(tmp, projection);

	mvStack.pop(); // initial matrix
	glDepthMask(GL_TRUE);

	SDL_GL_SwapWindow(window); // swap buffers

}


// Program entry point
int main(int argc, char *argv[]) {
	SDL_Window * hWindow; // window handle
	SDL_GLContext glContext; // OpenGL context handle
	hWindow = setupRC(glContext); // Create window and render context 

								  // Required on Windows *only* init GLEW to access OpenGL beyond 1.1
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err) { // glewInit failed, something is seriously wrong
		std::cout << "glewInit failed, aborting." << endl;
		exit(1);
	}
	cout << glGetString(GL_VERSION) << endl;

	init();

	bool running = true; // set running to true
	SDL_Event sdlEvent;  // variable to detect SDL events
	while (running) {	// the event loop
		while (SDL_PollEvent(&sdlEvent)) {
			if (sdlEvent.type == SDL_QUIT)
				running = false;
		}
		update();
		draw(hWindow); // call the draw function
	}

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(hWindow);
	SDL_Quit();
	return 0;
}