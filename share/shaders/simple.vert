// $Id$

uniform vec2 texSize;
uniform vec3 texStepX; // = vec3(vec2(1.0 / texSize.x), 0.0);
uniform float alpha;

varying vec2 scaled;
varying vec3 misc;

void main()
{
	gl_Position = ftransform();
	misc = vec3((vec2(0.5) + vec2(1.0, 0.0) * alpha) * texStepX.x, gl_MultiTexCoord0.t);
	scaled.x = gl_MultiTexCoord0.s * texSize.x;
	scaled.y = gl_MultiTexCoord0.t * texSize.y + 0.5;
}
