uniform mat4 u_mvpMatrix;
uniform vec3 texSize; // xy = texSize1,  xz = texSize2
uniform vec2 dstSize;

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec4 intCoord;
varying vec4 cornerCoord0;
varying vec4 cornerCoord1;

void main()
{
	gl_Position = u_mvpMatrix * a_position;

	float BIAS = 0.001f;
	vec3 texOffset = (0.5 + BIAS) / dstSize.xyy;
	texOffset.y *= texSize.z / texSize.y;
	vec3 texCoord = a_texCoord + texOffset;

	intCoord.xy = texCoord.xy * texSize.xy;
	intCoord.zw = -intCoord.xy;

	cornerCoord0 = vec4(texCoord.xy,
	                    texCoord.xy + 1.0 / texSize.xy);
	cornerCoord1 = vec4(texCoord.xz,
	                    texCoord.xz + 1.0 / texSize.xz);
}
