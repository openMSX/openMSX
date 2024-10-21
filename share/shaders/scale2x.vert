uniform mat4 u_mvpMatrix;
uniform vec3 texSize;

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec2 texStep; // could be uniform
varying vec2 coord2pi;
varying vec2 texCoord;
varying vec2 videoCoord;

const float pi = 3.1415927;
const float pi2 = 2.0 * pi;

void main()
{
	gl_Position = u_mvpMatrix * a_position;
	texCoord = a_texCoord.xy;
	coord2pi = a_texCoord.xy * texSize.xy * pi2;
	texStep = 1.0 / texSize.xy;

#if SUPERIMPOSE
	videoCoord = a_texCoord.xz;
#endif
}
