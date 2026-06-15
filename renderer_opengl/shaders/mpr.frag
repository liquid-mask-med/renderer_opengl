#version 460 core

in vec2 uv;

uniform sampler3D volumeTexture;

uniform vec3 origin;
uniform vec3 axisU;
uniform vec3 axisV;

uniform vec2 centerUV;
uniform vec2 halfUV;

uniform int windowCenter;
uniform int windowWidth;

uniform vec3 volumeSize;

//视口宽高
uniform int width;
uniform int height;

out vec4 color;

void main()
{
	float uOffset = centerUV.x + (uv.x - 0.5) * 2.0 * halfUV.x;
	float vOffset = centerUV.y + ((1 - uv.y) - 0.5) * 2.0 * halfUV.y;
	vec3 worldPos = origin + axisU * uOffset + axisV * vOffset;
	
	// 优化后的坐标变换：将 worldPos 映射到 [0,1] 的纹理坐标空间
	vec3 invVolumeSize = 1.0 / volumeSize;
	vec3 coord = worldPos * invVolumeSize + vec3(0.5);
	// 防止越界采样
	//coord = clamp(coord, vec3(0.0), vec3(1.0));
	if(coord.x <= 0 || coord.y <= 0 || coord.z <= 0 || coord.x >= 1 || coord.y >= 1 || coord.z >= 1) {
		discard;
	}

	//coord = vec3(uv.x, uv.y, 0.5);

	// 采样并恢复为 16-bit，再取高 12 位
	float s = texture(volumeTexture, coord).r; // 归一化 [0,1]
	const int MAX16 = 65535;   // (1 << 16) - 1
	int i16 = int(round(s * float(MAX16))); // 恢复到 [0,65535]
	//int i12 = i16 >> 4; // 取高 12 位，范围 [0,4095]

	const int MAX12 = 4095;

	int hu = i16 - 1024;
	
	int windowMax = windowCenter + windowWidth / 2;
	int windowMin = windowCenter - windowWidth / 2;

	float finalVal = float(hu - windowMin) / float(windowWidth);
	finalVal = clamp(finalVal, 0, 1);

	color = vec4(vec3(finalVal), 1.0);
}