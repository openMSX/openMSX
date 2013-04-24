uniform vec2 texSize;

varying vec4 posABCD;
varying vec4 posEL;
varying vec4 posGJ;
varying vec3 scaled;
varying vec2 videoCoord;

void main()
{
	gl_Position = ftransform();
	vec2 texStep = 1.0 / texSize;

	posABCD.xy = gl_MultiTexCoord0.st;
	posABCD.zw = gl_MultiTexCoord0.st + texStep * vec2( 1.0,  1.0);
	posEL.xy = gl_MultiTexCoord0.st + texStep * vec2( 0.0, -1.0);
	posEL.zw = gl_MultiTexCoord0.st + texStep * vec2( 1.0,  2.0);
	posGJ.xy = gl_MultiTexCoord0.st + texStep * vec2(-1.0,  0.0);
	posGJ.zw = gl_MultiTexCoord0.st + texStep * vec2( 2.0,  1.0);

	scaled.x =        gl_MultiTexCoord0.s  * texSize.x;
	scaled.y = (1.0 - gl_MultiTexCoord0.s) * texSize.x;
	scaled.z =        gl_MultiTexCoord0.t  * texSize.y;

#if SUPERIMPOSE
	videoCoord = gl_MultiTexCoord1.st;
#endif
}
