// $Id$

uniform vec2 texSize;

varying vec2 scaled;
varying float y;
varying float texStepX;

void main()
{
	gl_Position = ftransform();
	texStepX = 1.0 / texSize.x;
	scaled.x = gl_MultiTexCoord0.s * texSize.x;
	scaled.y = gl_MultiTexCoord0.t * texSize.y + 0.5;
	y = gl_MultiTexCoord0.t;
}
