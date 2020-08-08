uniform mat4 u_mvpMatrix;
uniform vec3 texSize;

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec2 leftTop;
varying vec2 edgePos;
varying vec4 misc;
varying vec2 videoCoord;

void main()
{
	gl_Position = u_mvpMatrix * a_position;

	edgePos = a_texCoord.xy * vec2(1.0, 2.0);

	vec2 texStep = 1.0 / texSize.xy;
	leftTop  = a_texCoord.xy - texStep;

	vec2 subPixelPos = edgePos * texSize.xy * vec2(1.0, 0.5);
	vec2 texStep2 = 2.0 * texStep;
	misc = vec4(subPixelPos, texStep2);

#if SUPERIMPOSE
	videoCoord = a_texCoord.xz;
#endif
}
