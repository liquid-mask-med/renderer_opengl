#version 460 core

layout (location = 0) in vec3 position;

uniform mat4 modelMatrix;
uniform mat4 projectMatrix;
uniform mat4 viewMatrix;

out vec3 volumePos;

void main()
{
	volumePos = position;
	gl_Position = projectMatrix * viewMatrix * modelMatrix * vec4(position, 1.0f);
}