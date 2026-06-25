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

uniform float stepSize;
uniform int maxSteps;

out vec4 color;

float SampleStored(vec3 coord);
float ApplyWindow(float sampledValue);
vec3 GetGradient(vec3 coord);
float InterleavedGradientNoise(vec2 pixel);
void main()
{	
	vec3 rayStep = viewRay * stepSize;
	vec3 pos = volumePos + rayStep * InterleavedGradientNoise(gl_FragCoord.xy);
	//vec3 pos = volumePos + rayStep;

	vec4 accumulatedColor = vec4(0.0);
	float eps = 1e-4;
	
	for(int i=0; i < maxSteps; ++i) {
		vec3 coord = pos / volumePhysicalSize + vec3(0.5);
		coord.z = 1 - coord.z;
	
		if(any(lessThan(coord, vec3(-eps))) || any(greaterThan(coord, vec3(1.0 + eps)))) {
			break;
		}
	
		float sampledValue = texture(volumeTexture, coord).r;
	
		vec4 sampledColor = texture(volumeColor, sampledValue * 65535.0 / 4095.0);
		float alpha = 1.0 - pow(max(1.0 - sampledColor.a, 0.0), stepSize);
		//alpha = sampledColor.a;
		

		if (alpha > 0.001)
		{
			// The scalar gradient points toward denser material, therefore its
			// negative is the outward-facing normal used for head lighting.
			//
			// coord.z is flipped above for texture addressing, while viewRay is
			// still in the renderer's physical/model space. Flip the normal's z
			// component back before comparing it with the view/light direction.
			vec3 N = -GetGradient(coord);
			N.z = -N.z;
			vec3 L = normalize(-viewRay);
		
			float ndotl = max(dot(N, L), 0.0);
			sampledColor.rgb *= 0.2 + 1.0 * ndotl;
		}
	
		accumulatedColor.rgb += (1.0 - accumulatedColor.a) * sampledColor.rgb * alpha;
		accumulatedColor.a += (1.0 - accumulatedColor.a) * alpha;
	
		if(accumulatedColor.a > 0.98) {
			break;
		}
	
		pos += rayStep;
	}
	
	//color = vec4(accumulatedColor.rgb, 1.0);
	color = accumulatedColor;

	//vec3 bgColor = vec3(0.55, 0.58, 0.78);
	vec3 bgColor = vec3(0);

	vec3 finalRgb =
		accumulatedColor.rgb +
		(1.0 - accumulatedColor.a) * bgColor;

	color = vec4(finalRgb, 1.0);

}

float InterleavedGradientNoise(vec2 pixel)
{
	return fract(52.9829189 * fract(dot(pixel, vec2(0.06711056, 0.00583715))));
}

float ApplyWindow(float sampledValue) {
	float raw = sampledValue * 65535.0;
	float hu = raw - 1024.0;

	float minValue = float(windowCenter) - float(windowWidth) * 0.5;
	float gray = (hu - minValue) / float(windowWidth);

	return clamp(gray, 0.0, 1.0);
}

vec3 GetGradient(vec3 coord)
{
    vec3 cellStep = 1.0 / volumePixelSize;

    float xp = texture(volumeTexture, coord + vec3(cellStep.x, 0.0, 0.0)).r * 65535.0;
    float xm = texture(volumeTexture, coord - vec3(cellStep.x, 0.0, 0.0)).r * 65535.0;

    float yp = texture(volumeTexture, coord + vec3(0.0, cellStep.y, 0.0)).r * 65535.0;
    float ym = texture(volumeTexture, coord - vec3(0.0, cellStep.y, 0.0)).r * 65535.0;

    float zp = texture(volumeTexture, coord + vec3(0.0, 0.0, cellStep.z)).r * 65535.0;
    float zm = texture(volumeTexture, coord - vec3(0.0, 0.0, cellStep.z)).r * 65535.0;

    vec3 spacing = volumePhysicalSize / volumePixelSize;

    vec3 grad = vec3(
        (xp - xm) / (2*spacing.x),
        (yp - ym) / (2*spacing.y),
        (zp - zm) / (2*spacing.z)
    );

    float len = length(grad);
    return len > 0.0 ? grad / len : vec3(0.0);
}

float SampleStored(vec3 coord)
{
    return texture(volumeTexture, coord).r * 65535.0;
}
