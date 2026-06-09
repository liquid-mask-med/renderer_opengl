#version 460 core

layout (location = 0) in vec3 position;

out vec2 uv;

void main()
{
	gl_Position = vec4(position.x, position.y,0.0,1.0f);
	uv = vec2(position.xy * 0.5 + 0.5);
}