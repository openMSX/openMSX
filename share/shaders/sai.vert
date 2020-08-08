uniform mat4 u_mvpMatrix;
uniform vec3 texSize;

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec4 posABCD;
varying vec4 posEL;
varying vec4 posGJ;
varying vec3 scaled;
varying vec2 videoCoord;

void main()
{
	gl_Position = u_mvpMatrix * a_position;
	vec2 texStep = 1.0 / texSize.xy;

	posABCD.xy = a_texCoord.xy;
	posABCD.zw = a_texCoord.xy + texStep * vec2( 1.0,  1.0);
	posEL.xy = a_texCoord.xy + texStep * vec2( 0.0, -1.0);
	posEL.zw = a_texCoord.xy + texStep * vec2( 1.0,  2.0);
	posGJ.xy = a_texCoord.xy + texStep * vec2(-1.0,  0.0);
	posGJ.zw = a_texCoord.xy + texStep * vec2( 2.0,  1.0);

	scaled.x =        a_texCoord.x  * texSize.x;
	scaled.y = (1.0 - a_texCoord.x) * texSize.x;
	scaled.z =        a_texCoord.y  * texSize.y;

#if SUPERIMPOSE
	videoCoord = a_texCoord.xz;
#endif
}
