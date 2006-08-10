// $Id$

uniform vec2 texSize;

varying vec2 left;
varying vec2 mid;
varying vec2 right;
varying vec2 edgePos;
varying vec2 weightPos;

void main()
{
	gl_Position = ftransform();
	
	vec2 texStep = vec2(1.0 / texSize.x, 0.0);
	mid   = gl_MultiTexCoord0.st;
	left  = gl_MultiTexCoord0.st - texStep;
	right = gl_MultiTexCoord0.st + texStep;

	edgePos.x = gl_MultiTexCoord0.s;
	edgePos.y = gl_MultiTexCoord0.t * 2.0;
	weightPos = edgePos * texSize;
}
