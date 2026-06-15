#pragma once
#include "common.h"
#include <glm/matrix.hpp>
#include <mutex>
#include "RenderBox.h"

#define RENDERER_API __declspec(dllexport)

class Renderer
{
private:
	GLFWwindow* window = nullptr;
	bool snapshot = false;

	RenderImage image3D;

	RenderParameters renderParams;
	RenderBox renderBox;

	GLuint shaderProgram;

	GLuint VAO, mprVAO, VBO, mprVBO, EBO;

	GLuint volumeTexture;
	GLuint volumeColor;

	glm::mat4 modelMatrix;
	glm::mat4 viewMatrix;
	glm::mat4 projectMatrix;

	GLuint mprShader;
	GLuint mprFBO;
	GLuint mprTexture;

	SliceDesc sliceStates[3];
	RenderImage sliceImage[3];


	GLuint fboIds[4]{0};
	GLuint fboTextures[4]{0};
	GLuint depthRbo;

	int viewW[4]{ 0 };
	int viewH[4]{ 0 };

	std::mutex dataMutex;
	std::mutex canvasMutex[4];

	void updateProjection();
public:
	Renderer();

	~Renderer();

	void init();

	bool initOpenGL();

	void getImage(RenderImage* image);
	void getSliceImage(int index, RenderImage* image);

	void updateRenderParameters(RenderParameters newParam);

	void render(int mask);

	void resizeViewport(int viewIndex, int width, int height);

	void enableSnapshot() { snapshot = true; }
	void disableSnapshot() { snapshot = false; }
	void updateSlice(int index, glm::vec3 origin, glm::vec3 axisU, glm::vec3 axisV, SliceDisplayMapping mapping);

	void rotateCamera(float xDeg, float yDeg);
	void scaleCamera(float scaleFactor);
};

extern "C" {
	RENDERER_API Renderer* CreateRenderer();
	RENDERER_API void DeleteRenderer(Renderer* p);
	RENDERER_API void Init(Renderer* p);
	RENDERER_API void SetUpRenderParameters(Renderer* p, uint16_t* volumeData, int width, int height, int depth, int windowWidth, int windowCenter, double spacing, double thickness);
	RENDERER_API void Render(Renderer* p, int mask);
	RENDERER_API void GetImage(Renderer* p, RenderImage* image);
	RENDERER_API void GetSliceImage(Renderer* p, int index, RenderImage* image);
	RENDERER_API void EnableSnapshot(Renderer* p);
	RENDERER_API void DisableSnapshot(Renderer* p);
	RENDERER_API void ResizeViewport(Renderer* p, int viewIndex, int width, int height);
	RENDERER_API void SetUpSliceState(Renderer* p, int index, Vec3 origin, Vec3 axisU, Vec3 axisV, SliceDisplayMapping mapping);
	RENDERER_API void RotateCamera(Renderer* p, float dx, float dy);
	RENDERER_API void ScaleCamera(Renderer* p, float scaleFactor);
}

