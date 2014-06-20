uniform mat4 u_mvpMatrix;
uniform vec3 texSize; // xy = texSize1,  xz = texSize2

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec4 intCoord;
varying vec4 cornerCoord0;
varying vec4 cornerCoord1;

void main()
{
	gl_Position = u_mvpMatrix * a_position;
	intCoord.xy = a_texCoord.xy * texSize.xy;
	intCoord.zw = -intCoord.xy;
	cornerCoord0 = vec4(a_texCoord.xy,
	                    a_texCoord.xy + 1.0 / texSize.xy);
	cornerCoord1 = vec4(a_texCoord.xz,
	                    a_texCoord.xz + 1.0 / texSize.xz);
}
