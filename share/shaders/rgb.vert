uniform mat4 u_mvpMatrix;
uniform vec3 texSize;

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec4 scaled;
varying vec2 pos;
varying vec2 videoCoord;

void main()
{
	gl_Position = u_mvpMatrix * a_position;
	vec2 tmp = a_texCoord.xy * texSize.xy;
	scaled = tmp.xxxy + vec4(0.0, 1.0/3.0, 2.0/3.0, 0.5);
	pos        = a_texCoord.xy;
#if SUPERIMPOSE
	videoCoord = a_texCoord.xz;
#endif
}
