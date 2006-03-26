// $Id$

const vec2 texSize = vec2(1024.0, 512.0);
const vec2 texStep = 1.0 / texSize;
varying vec4 cornerCoord;

void main()
{
	gl_Position = ftransform();
	cornerCoord = vec4(
		gl_MultiTexCoord0.xy,
		gl_MultiTexCoord0.xy + texStep
		);
}
