// $Id$

uniform vec2 texSize;
uniform vec3 texStepX; // = vec3(vec2(1.0 / texSize.x), 0.0);
uniform vec4 cnst;     // = vec4(scan_a, scan_b, scan_c, alpha);

varying vec2 scaled;
varying vec3 misc;

void main()
{
	const float alpha  = cnst.w;

	gl_Position = ftransform();
	misc = vec3((vec2(0.5) + vec2(1.0, 0.0) * alpha) * texStepX.x, gl_MultiTexCoord0.t);
	scaled.x = gl_MultiTexCoord0.s * texSize.x;
	scaled.y = gl_MultiTexCoord0.t * texSize.y + 0.5;
}
