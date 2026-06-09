#version 460 core

in vec3 volumePos;

uniform sampler3D volumeTexture;
uniform sampler1D volumeColor;

uniform mat4 modelMatrix;
uniform mat4 projectMatrix;
uniform mat4 viewMatrix;

uniform vec3 viewRay;

//窗宽窗位
uniform int windowCenter;
uniform int windowWidth;

//屏幕宽高
uniform int width;
uniform int height;

//体数据的物理尺寸
uniform vec3 volumePhysicalSize;

//体数据像素尺寸
uniform vec3 volumePixelSize;

uniform int maxSteps;

out vec4 color;

//struct Ray {
//	vec3 origin;
//	vec3 direction;
//	float min;
//	float max;
//}
//
//struct Intersection {
//	float min;
//	float max;
//}
//
//struct AABB {
//	vec3 min;
//	vec3 max;
//}
//
//struct VolumeDesc {
//	AABB boundingBox;
//	float stepSize;
//	float densityScale;
//}

//Intersection IntersectAABB(Ray ray, AABB aabb) {
//	Intersection intersect;
//	vec3 invR = vec3(1.0) / ray.direction;
//	vec3 bot = invR * (aabb.min - ray.origin);
//	vec3 top = invR * (aabb.max - ray.origin);
//	vec3 tmin = min(top. bot);
//	vec3 tmax = max(top, bot);
//
//	float largestMin = max(max(tmin.x, tmin.y), tmin.z);
//	float smallestMax = min(min(tmax.x, tmax.y), tmax.z);
//
//	intersect.min = largestMin;
//	intersect.max = smallestMax;
//	return intersect;
//}
//
//vec3 GetNormalizedTexCoord(vec3 position, AABB aabb) {
//	vec3 v1 = position - aabb.min;
//	vec3 v2 = aabb.max - aabb.min;
//	return vec3(float(v1.x/v2.x), float(v1.y/v2.y), float(v1.z/v2.z));
//}
//
//float GetVolume(vec3 texCoord) {
//	return texture(volumeTexture, texCoord.xyz).r;
//}



//void rayCast(Ray ray, float t, float maxT, VolumeDesc desc, vec3 viewRay) {
//	
//	vec4 accumulatedColor = vec4(0.0);
//
//	int maxSteps = 1500;
//
//	float distance = 0;
//	
//	bool depthWrite = false;
//
//	gl_FragDepth = 1.0;
//
//	for(int iter = 0; iter < maxSteps; iter++) {
//		if(t > maxT) {
//			break;
//		}
//
//		vec3 position = ray.origin + t * ray.direction;
//		vec3 coord = GetNormalizedTexCoord(position, desc.boundingBox);
//
//		float sampledPixel = GetVolume(coord);
//
//		// 采样并恢复为 16-bit，再取高 12 位
//		float s = texture(volumeTexture, coord).r; // 归一化 [0,1]
//		const int MAX16 = 65535;   // (1 << 16) - 1
//		int i16 = int(round(s * float(MAX16))); // 恢复到 [0,65535]
//		vec4 color = vec4(sampledPixel);
//
//	}
//
//}

float ApplyWindow(float sampledValue);

void main()
{	
	//Ray ray;
	//ray.origin = nearPos;
	//ray.direction = vec3(viewRay);
	//ray.min = 0.0;
	//ray.max = 10000.0;
	//
	//float volumeWidth = volumePhysicalSize.x;
	//float volumeHeight = volumePhysicalSize.y;
	//float volumeDepth = volumePhysicalSize.z;
	//
	//VolumeDesc desc;
	//desc.boundingBox = AABB((modelMatrix * vec4(-volumePhysicalSize.xyz/2, 1.0).xyz), modelMatrix * vec4(volumePhysicalSize.xyz/2, 1.0));
	//desc.stepSize = 0.4;
	//desc.densityScale = 100.0;
	//
	//Intersection intersect = IntersectAABB(ray, desc.boundingBox);
	//if(intersect.max < intersect.min) {
	//	discard;
	//}
	//
	//float minT = max(intersect.min, ray.min);
	//float maxT = min(intersect.max, ray.max);
	//float t = minT;
	//t -= 0.5 * desc.stepSize;
	//
	//rayCast(ray, t, maxT, desc, viewRay);
	//if(gl_FragColor.a < 0.75) {
	//	discard;
	//}

	vec3 pos = volumePos;
	vec4 accumulatedColor = vec4(0.0);
	float eps = 1e-4;
	
	for(int i=0; i < maxSteps; ++i) {
		vec3 coord = pos / volumePhysicalSize + vec3(0.5);
		coord.z = 1 - coord.z;
	
		if(any(lessThan(coord, vec3(-eps))) || any(greaterThan(coord, vec3(1.0 + eps)))) {
			break;
		}
	
		float sampledValue = texture(volumeTexture, coord).r;
		//float gray = ApplyWindow(sampledValue);
	
		vec4 sampledColor = texture(volumeColor, sampledValue * 65535.0 / 4095.0);
		float alpha = sampledColor.a;
	
		accumulatedColor.rgb += (1.0 - accumulatedColor.a) * sampledColor.rgb * alpha;
		accumulatedColor.a += (1.0 - accumulatedColor.a) * alpha;
	
		if(accumulatedColor.a > 0.98) {
			break;
		}
	
		pos += viewRay;
	}
	
	color = vec4(accumulatedColor.rgb, 1.0);

}

float ApplyWindow(float sampledValue) {
	float raw = sampledValue * 65535.0;
	float hu = raw - 1024.0;

	float minValue = float(windowCenter) - float(windowWidth) * 0.5;
	float gray = (hu - minValue) / float(windowWidth);

	return clamp(gray, 0.0, 1.0);
}