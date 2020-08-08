uniform mat4 u_mvpMatrix;
uniform vec3 texSize; // not used

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec2 texCoord;
varying vec2 videoCoord;

void main()
{
	gl_Position = u_mvpMatrix * a_position;
	texCoord   = a_texCoord.xy;
#if SUPERIMPOSE
	videoCoord = a_texCoord.xz;
#endif
}
