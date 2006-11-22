// $Id$

uniform vec2 texSize;

varying vec2 leftTop;
varying vec2 edgePos;
varying vec4 misc;

void main()
{
	gl_Position = ftransform();

	edgePos = gl_MultiTexCoord0.st * vec2(1.0, 2.0);
	
	vec2 texStep = vec2(1.0 / texSize.x, 0.5 / texSize.y);
	leftTop  = gl_MultiTexCoord0.st - texStep;

	vec2 subPixelPos = edgePos * texSize;
	vec2 texStep2 = 2.0 * texStep;
	misc = vec4(subPixelPos, texStep2);
}
