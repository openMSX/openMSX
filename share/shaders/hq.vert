// $Id$

uniform vec2 texSize;

varying vec2 mid;
varying vec2 leftTop;
varying vec2 edgePos;
varying vec2 weightPos;
varying vec2 texStep2; // could be uniform
varying vec2 videoCoord;

void main()
{
	gl_Position = ftransform();
	
	vec2 texStep = 1.0 / texSize;
	mid     = gl_MultiTexCoord0.st;
	leftTop = gl_MultiTexCoord0.st - texStep;

	edgePos.x = gl_MultiTexCoord0.s;
	edgePos.y = gl_MultiTexCoord0.t * 2.0;
	vec2 texSizeB = vec2(texSize.x, texSize.y * 0.5);
	weightPos = edgePos * texSizeB;

	texStep2 = 2.0 * texStep;

	videoCoord = gl_MultiTexCoord1.st;
}
