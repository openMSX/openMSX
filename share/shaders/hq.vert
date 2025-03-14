uniform mat4 u_mvpMatrix;
uniform vec3 texSize;
uniform vec2 edgePosScale;

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec2 mid;
varying vec2 leftTop;
varying vec2 edgePos;
varying vec2 weightPos;
varying vec2 texStep2; // could be uniform
varying vec2 videoCoord;

void main()
{
	gl_Position = u_mvpMatrix * a_position;

	vec2 texStep = 1.0 / texSize.xy;
	mid     = a_texCoord.xy;
	leftTop = a_texCoord.xy - texStep;

	edgePos = a_texCoord.xy * edgePosScale;
	weightPos = a_texCoord.xy * texSize.xy;

	texStep2 = 2.0 * texStep;

#if SUPERIMPOSE
	videoCoord = a_texCoord.xz;
#endif
}
