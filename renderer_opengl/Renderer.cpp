#define GLEW_STATIC
#include <GL/glew.h>

#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <Windows.h>

#include "Renderer.h"
#include "TransferFuction.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

#define SCREEN_VERTEX_COUNT 6

static std::filesystem::path moduleDirectory()
{
	HMODULE module = nullptr;
	GetModuleHandleExW(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		reinterpret_cast<LPCWSTR>(&moduleDirectory),
		&module);

	wchar_t path[MAX_PATH]{};
	GetModuleFileNameW(module, path, MAX_PATH);
	return std::filesystem::path(path).parent_path();
}

static bool loadShaderSource(const std::string& path, std::string& out) {
	const std::filesystem::path shaderPath = moduleDirectory() / path;
	std::ifstream file(shaderPath);
	if (!file.is_open()) {
		std::cerr << "Failed to load shader file: " << shaderPath << std::endl;
		return false;
	}

	std::stringstream ss;
	ss << file.rdbuf();
	out = ss.str();
	return true;
}

static void ensureImageBuffer(RenderImage& img, int inputWidth, int inputHeight) {
	int newLength = inputWidth * inputHeight * 4;
	if (img.length != newLength) {
		free(img.front);
		free(img.back);

		img.front = malloc(newLength);
		img.back = malloc(newLength);

		memset(img.front, 0, newLength);
		memset(img.back, 0, newLength);
	}

	img.width = inputWidth;
	img.height = inputHeight;
	img.length = newLength;
}

void Renderer::updateProjection()
{
	if (viewW[3] <= 0 || viewH[3] <= 0) return;

	float aspect = float(viewW[3]) / float(viewH[3]);

	float xLength = renderParams.width * renderParams.spacingX;
	float yLength = renderParams.height * renderParams.spacingY;
	float zLength = renderParams.depth * renderParams.spacingZ;

	float maxSide = std::max({ xLength, yLength, zLength });
	float viewRadius = maxSide * 0.6f;

	float left, right, bottom, top;

	if (aspect >= 1.0f)
	{
		// 窗口更宽，横向显示范围也要变宽
		left = -viewRadius * aspect;
		right = viewRadius * aspect;
		bottom = -viewRadius;
		top = viewRadius;
	}
	else
	{
		// 窗口更高，纵向显示范围也要变高
		left = -viewRadius;
		right = viewRadius;
		bottom = -viewRadius / aspect;
		top = viewRadius / aspect;
	}

	projectMatrix = glm::ortho(
		left,
		right,
		bottom,
		top,
		-viewRadius * 4.0f,
		viewRadius * 4.0f
	);
}

Renderer::Renderer() {
	//TODO..

	//init();
}

Renderer::~Renderer()
{
	glfwTerminate();
	window = nullptr;

	free(image3D.front);
	free(image3D.back);

	for (int i=0; i<3; i++)
	{
		free(sliceImage[i].front);
		free(sliceImage[i].back);
	}
}

void Renderer::init()
{
	//TODO..

	initOpenGL();

	//TODO..
}

bool Renderer::initOpenGL()
{
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

	window = glfwCreateWindow(1, 1, "OpenGL", nullptr, nullptr);
	if (window == nullptr) {
		std::cout << "Failed to create window" << std::endl;
		glfwTerminate();
		return false;
	}

	glfwSetWindowSizeLimits(window, 1, 1, GLFW_DONT_CARE, GLFW_DONT_CARE);
	glfwMakeContextCurrent(window);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		std::cout << "Failed to initialize GLEW" << std::endl; 
		glfwTerminate();
		return false;
	}

	glGenFramebuffers(4, fboIds);
	glGenTextures(4, fboTextures);

	glGenRenderbuffers(1, &depthRbo);

	//main shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);

	std::string vertexShaderSource;
	loadShaderSource("shaders/main.vs", vertexShaderSource);
	const char* pVertexShaderSource = vertexShaderSource.c_str();
	glShaderSource(vertexShader, 1, &pVertexShaderSource, nullptr);
	glCompileShader(vertexShader);

	GLint compileSuccess;
	GLchar infoLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compileSuccess);
	if (!compileSuccess) {
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cerr << "Compile vertex shader error: " << infoLog << std::endl;
	}

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

	std::string fragmentShaderSource;
	loadShaderSource("shaders/main.frag", fragmentShaderSource);
	const char* pFragmentShaderSource = fragmentShaderSource.c_str();

	glShaderSource(fragmentShader, 1, &pFragmentShaderSource, nullptr);
	glCompileShader(fragmentShader);

	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compileSuccess);
	if (!compileSuccess) {
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cerr << "Compile fragment shader error: " << infoLog << std::endl;
	}

	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);

	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &compileSuccess);
	if (!compileSuccess) {
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		std::cerr << "Link program error: " << infoLog << std::endl;
	}

	//mpr shader
	loadShaderSource("shaders/mpr.vs", vertexShaderSource);
	pVertexShaderSource = vertexShaderSource.c_str();
	glShaderSource(vertexShader, 1, &pVertexShaderSource, nullptr);
	glCompileShader(vertexShader);

	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compileSuccess);
	if (!compileSuccess) {
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		std::cerr << "Compile mpr vertex shader error: " << infoLog << std::endl;
	}

	loadShaderSource("shaders/mpr.frag", fragmentShaderSource);
	pFragmentShaderSource = fragmentShaderSource.c_str();

	glShaderSource(fragmentShader, 1, &pFragmentShaderSource, nullptr);
	glCompileShader(fragmentShader);

	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &compileSuccess);
	if (!compileSuccess) {
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		std::cerr << "Compile mpr fragment shader error: " << infoLog << std::endl;
	}

	mprShader = glCreateProgram();
	glAttachShader(mprShader, vertexShader);
	glAttachShader(mprShader, fragmentShader);
	glLinkProgram(mprShader);

	glGetProgramiv(mprShader, GL_LINK_STATUS, &compileSuccess);
	if (!compileSuccess) {
		glGetProgramInfoLog(mprShader, 512, NULL, infoLog);
		std::cerr << "Link mpr program error: " << infoLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glGenTextures(1, &volumeTexture);
	glGenTextures(1, &volumeColor);

	GLfloat mprVertices[] = {
		-1.0, 1.0, 0.0,
		-1.0,-1.0, 0.0,
		 1.0, 1.0, 0.0,
		 1.0, 1.0, 0.0,
		-1.0,-1.0, 0.0,
		 1.0,-1.0, 0.0
	};

	glGenVertexArrays(1, &mprVAO);
	glGenBuffers(1, &mprVBO);

	glBindVertexArray(mprVAO);

	glBindBuffer(GL_ARRAY_BUFFER, mprVBO);
	glBufferData(GL_ARRAY_BUFFER, 3 * SCREEN_VERTEX_COUNT * sizeof(GLfloat), mprVertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	return true;
}

void Renderer::getImage(RenderImage* p)
{
	if (p && image3D.width != 0 && image3D.height != 0) {
		p->width = image3D.width;
		p->height = image3D.height;
		p->front = image3D.front;
		p->back = nullptr;
		p->length = image3D.length;
	}
}

void Renderer::getSliceImage(int index, RenderImage* p)
{
	if (p && sliceImage[index].width != 0 && sliceImage[index].height != 0) {
		p->width = sliceImage[index].width;
		p->height = sliceImage[index].height;
		p->front = sliceImage[index].front;
		p->back = nullptr;
		p->length = sliceImage[index].length;
	}
}

void Renderer::updateRenderParameters(RenderParameters newParam)
{
	this->renderParams = newParam;

	float xLength = renderParams.width * renderParams.spacingX;
	float yLength = renderParams.height * renderParams.spacingY;
	float zLength = renderParams.depth * renderParams.spacingZ;

	renderBox = RenderBox(xLength, yLength, zLength);

	GLfloat* vertices = renderBox.getVertices();
	GLuint* indices = renderBox.getIndices();

	//modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(-xLength, -yLength, -zLength) * 0.5f);
	modelMatrix = glm::mat4(1.0);
	float maxSide = std::max({ xLength, yLength, zLength });
	float viewRadius = maxSide * 0.6;
	viewMatrix = glm::lookAt(glm::vec3(0, -yLength, 0), glm::vec3(), glm::vec3(0, 0, 1));
	projectMatrix = glm::ortho(-viewRadius, viewRadius, -viewRadius, viewRadius, -viewRadius * 2, viewRadius * 2);
	updateProjection();

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, BOX_VERTEX_COUNT * sizeof(GLfloat), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, BOX_INDEX_COUNT * sizeof(GLuint), indices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glBindTexture(GL_TEXTURE_3D, volumeTexture);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage3D(GL_TEXTURE_3D, 0, GL_R16, renderParams.width, renderParams.height, renderParams.depth, 0, GL_RED, GL_UNSIGNED_SHORT, renderParams.volumeData);
	glBindTexture(GL_TEXTURE_3D, 0);

	glBindTexture(GL_TEXTURE_1D, volumeColor);

	// 从 transfer function 构建 LUT
	auto lut = TransferFuction::BuildRGBA_LUT(); // 返回 std::array<RGBA, 4096>

	// 假设 lut 是 std::array<RGBA, 4096>，RGBA 的通道为 float（0.0f-1.0f）
	std::vector<float> floatData;
	floatData.reserve(lut.size() * 4);
	for (const auto& c : lut) {
		floatData.push_back(c.r);
		floatData.push_back(c.g);
		floatData.push_back(c.b);
		floatData.push_back(c.a);
	}

	// 纹理参数
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// 对齐（float = 4 bytes）
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	// 上传为 1D 浮点纹理：内部格式 GL_RGBA32F，外部格式 GL_RGBA，数据类型 GL_FLOAT
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA32F, static_cast<GLsizei>(lut.size()), 0, GL_RGBA, GL_FLOAT, floatData.data());

	glBindTexture(GL_TEXTURE_1D, 0);
}

void Renderer::render(int mask)
{
	RenderParameters localParams;
	glm::mat4 localModel, localView, localProj;
	SliceDesc localSlices[3];

	{
		std::lock_guard<std::mutex> dataLock(dataMutex);

		localParams = this->renderParams;
		localModel = this->modelMatrix;
		localView = this->viewMatrix;
		localProj = this->projectMatrix;

		for (int i=0; i<3; i++)
		{
			localSlices[i] = this->sliceStates[i];
		}
	}

	if ((mask & (1 << 3)) && viewW[3] > 0 && viewH[3] > 0) {
		glBindFramebuffer(GL_FRAMEBUFFER, fboIds[3]);
		glViewport(0, 0, viewW[3], viewH[3]);

		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);

		glDisable(GL_CULL_FACE);

		glClearColor(0, 0, 0, 1);
		//glClearColor(0.55f, 0.58f, 0.78f, 1.0f);
		glClearDepth(1.0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glUseProgram(shaderProgram);

		// 绑定体数据到纹理单元 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_3D, volumeTexture);
		glUniform1i(glGetUniformLocation(shaderProgram, "volumeTexture"), 0);

		// 绑定 transfer function (1D) 到纹理单元 1，并设置 uniform
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_1D, volumeColor);
		glUniform1i(glGetUniformLocation(shaderProgram, "volumeColor"), 1);

		glActiveTexture(GL_TEXTURE0);

		glUniform1i(glGetUniformLocation(shaderProgram, "windowCenter"), localParams.windowCenter);
		glUniform1i(glGetUniformLocation(shaderProgram, "windowWidth"), localParams.windowWidth);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "modelMatrix"), 1, GL_FALSE, glm::value_ptr(localModel));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projectMatrix"), 1, GL_FALSE, glm::value_ptr(localProj));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "viewMatrix"), 1, GL_FALSE, glm::value_ptr(localView));

		glm::vec3 viewRay(0, 0, -1);
		viewRay = glm::normalize(glm::vec3(glm::inverse(localView) * glm::vec4(viewRay, 0.0)));
		glUniform3fv(glGetUniformLocation(shaderProgram, "viewRay"), 1, value_ptr(viewRay));

		glUniform1i(glGetUniformLocation(shaderProgram, "width"), viewW[3]);
		glUniform1i(glGetUniformLocation(shaderProgram, "height"), viewH[3]);
		glUniform3fv(glGetUniformLocation(shaderProgram, "volumePhysicalSize"),1,value_ptr(glm::vec3(localParams.width * localParams.spacingX, localParams.height * localParams.spacingY, localParams.depth * localParams.spacingZ)));
		glUniform3fv(glGetUniformLocation(shaderProgram, "volumePixelSize"), 1, value_ptr(glm::vec3(localParams.width ,localParams.height ,localParams.depth)));

		float xLength = localParams.width * localParams.spacingX;
		float yLength = localParams.height * localParams.spacingY;
		float zLength = localParams.depth * localParams.spacingZ;

		float stepSize = std::min({ localParams.spacingX, localParams.spacingY, localParams.spacingZ });
		stepSize *= 0.5f;
		glUniform1f(glGetUniformLocation(shaderProgram, "stepSize"), stepSize);

		int maxSteps = (int)ceil(sqrt(xLength * xLength + yLength * yLength + zLength * zLength) / stepSize) + 1;
		glUniform1i(glGetUniformLocation(shaderProgram, "maxSteps"), maxSteps);

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		{
			std::lock_guard<std::mutex> canvasLock(canvasMutex[3]);

			ensureImageBuffer(image3D, viewW[3], viewH[3]);

			glReadPixels(0, 0, viewW[3], viewH[3], GL_BGRA, GL_UNSIGNED_BYTE, image3D.back);
			if (glGetError() != GL_NO_ERROR) {
				std::cout << "Error reading pixels" << std::endl;
			}

			std::swap(image3D.front, image3D.back);
		}
	}
	
	glDisable(GL_DEPTH_TEST);

	glUseProgram(mprShader);

	//glActiveTexture(GL_TEXTURE0);
	//glBindTexture(GL_TEXTURE_3D, volumeTexture);
	//glUniform1i(glGetUniformLocation(mprShader, "volumeTexture"), 0);
	//glUniform1i(glGetUniformLocation(mprShader, "windowCenter"), localParams.windowCenter);
	//glUniform1i(glGetUniformLocation(mprShader, "windowWidth"), localParams.windowWidth);

	glBindVertexArray(mprVAO);
	for (int i = 0; i < 3; i++)
	{

		if ((mask & (1 << i)) && viewW[i] > 0 && viewH[i] > 0) {
			glBindFramebuffer(GL_FRAMEBUFFER, fboIds[i]);
			glViewport(0, 0, viewW[i], viewH[i]);
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_3D, volumeTexture);
			glUniform1i(glGetUniformLocation(mprShader, "volumeTexture"), 0);

			glUniform1i(glGetUniformLocation(mprShader, "windowCenter"), localParams.windowCenter);
			glUniform1i(glGetUniformLocation(mprShader, "windowWidth"), localParams.windowWidth);
			glUniform1i(glGetUniformLocation(mprShader, "width"), viewW[i]);
			glUniform1i(glGetUniformLocation(mprShader, "height"), viewH[i]);
			glUniform3fv(glGetUniformLocation(mprShader, "origin"), 1, glm::value_ptr(localSlices[i].origin));
			glUniform3fv(glGetUniformLocation(mprShader, "axisU"), 1, glm::value_ptr(localSlices[i].axisU));
			glUniform3fv(glGetUniformLocation(mprShader, "axisV"), 1, glm::value_ptr(localSlices[i].axisV));
			glUniform2f(glGetUniformLocation(mprShader, "centerUV"), localSlices[i].mapping.centerU, localSlices[i].mapping.centerV);
			glUniform2f(glGetUniformLocation(mprShader, "halfUV"), localSlices[i].mapping.halfU, localSlices[i].mapping.halfV);

			float xLength = renderParams.width * renderParams.spacingX;
			float yLength = renderParams.height * renderParams.spacingY;
			float zLength = renderParams.depth * renderParams.spacingZ;
			glUniform3fv(glGetUniformLocation(mprShader, "volumeSize"), 1, glm::value_ptr(glm::vec3(xLength, yLength, zLength)));

			glDrawArrays(GL_TRIANGLES, 0, SCREEN_VERTEX_COUNT);

			{
				std::lock_guard<std::mutex> canvasLock(canvasMutex[i]);

				ensureImageBuffer(sliceImage[i], viewW[i], viewH[i]);

				glReadPixels(0, 0, viewW[i], viewH[i], GL_BGRA, GL_UNSIGNED_BYTE, sliceImage[i].back);
				if (glGetError() != GL_NO_ERROR) {
					std::cout << "Error reading slice pixels, index: " << i << std::endl;
				}

				std::swap(sliceImage[i].front, sliceImage[i].back);
			}
		}
	}
	glBindVertexArray(0);

	//glfwTerminate();
}


void Renderer::resizeViewport(int viewIndex, int inputWidth, int inputHeight)
{
	glfwMakeContextCurrent(window);

	viewW[viewIndex] = inputWidth;
	viewH[viewIndex] = inputHeight;

 	glBindTexture(GL_TEXTURE_2D, fboTextures[viewIndex]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, viewW[viewIndex], viewH[viewIndex], 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, fboIds[viewIndex]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTextures[viewIndex], 0);

	if (viewIndex == 3) {

		updateProjection();

		glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, inputWidth, inputHeight);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {

		std::cout << "FBO " << viewIndex
		          << " (Size: " << inputWidth << "x" << inputHeight << ") allocation failed!"
		          << " status=0x" << std::hex << status << std::dec
		          << " (" << static_cast<unsigned int>(status) << ")"
		          << " | fboId=" << fboIds[viewIndex]
		          << " | texId=" << fboTextures[viewIndex]
		          << std::endl;

	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	/*glfwSetWindowSize(window, width, height);
	glfwPollEvents();
	glfwGetFramebufferSize(window, &this->width, &this->height);*/
	//snapshot = true;
}

void Renderer::updateSlice(int index, glm::vec3 origin, glm::vec3 axisU, glm::vec3 axisV, SliceDisplayMapping mapping)
{
	 sliceStates[index] = { origin, axisU, axisV, mapping };
}

void Renderer::rotateCamera(float xDeg, float yDeg)
{
	glm::mat4 cameraMatrix = glm::inverse(viewMatrix);
	
	glm::vec3 up = glm::normalize(glm::vec3(cameraMatrix[1][0], cameraMatrix[1][1], cameraMatrix[1][2]));
	glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0), xDeg, up);
	cameraMatrix = rotationMatrix * cameraMatrix;

	glm::vec3 right = glm::normalize(glm::vec3(cameraMatrix[0][0], cameraMatrix[0][1], cameraMatrix[0][2]));
	rotationMatrix = glm::rotate(glm::mat4(1.0), yDeg, right);
	cameraMatrix = rotationMatrix * cameraMatrix;

	viewMatrix = glm::inverse(cameraMatrix);
	
}

void Renderer::scaleCamera(float scaleFactor)
{

}

RENDERER_API Renderer* CreateRenderer()
{
	return new Renderer();
}

RENDERER_API void DeleteRenderer(Renderer* p)
{
	delete p;
}

RENDERER_API void Render(Renderer* p, int mask)
{
	return p->render(mask);
}

RENDERER_API void GetImage(Renderer* p, RenderImage* image)
{
	return p->getImage(image);
}

RENDERER_API void GetSliceImage(Renderer* p, int index, RenderImage* image)
{
	return p->getSliceImage(index, image);
}

RENDERER_API void Init(Renderer* p)
{
	return p->init();
}

RENDERER_API void EnableSnapshot(Renderer* p)
{
	p->enableSnapshot();
}

RENDERER_API void DisableSnapshot(Renderer* p)
{
	p->disableSnapshot();
}

RENDERER_API void ResizeViewport(Renderer* p, int index, int width, int height)
{
	p->resizeViewport(index, width, height);
}

RENDERER_API void SetUpRenderParameters(Renderer* p, uint16_t* volumeData, int width, int height, int depth, int windowWidth, int windowCenter, double spacingX, double spacingY, double spacingZ)
{
	p->updateRenderParameters({ width, height, depth, volumeData, spacingX, spacingY, spacingZ, windowCenter, windowWidth });
}

RENDERER_API void SetUpSliceState(Renderer* p, int index, Vec3 origin, Vec3 axisU, Vec3 axisV, SliceDisplayMapping mapping)
{
	glm::vec3 o(origin.x, origin.y, origin.z);
	glm::vec3 u(axisU.x, axisU.y, axisU.z);
	glm::vec3 v(axisV.x, axisV.y, axisV.z);

	p->updateSlice(index, o, glm::normalize(u), glm::normalize(v), mapping);
}

RENDERER_API void RotateCamera(Renderer* p, float dx, float dy)
{
	p->rotateCamera(-glm::radians(dx), glm::radians(dy));
}

RENDERER_API void ScaleCamera(Renderer* p, float scaleFactor)
{
	p->scaleCamera(scaleFactor);
}
