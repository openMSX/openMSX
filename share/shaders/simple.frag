// $Id$

uniform sampler2D tex;
uniform float alpha;
uniform float scan_a;
uniform float scan_b;
uniform float scan_c;
uniform vec3 texStepX; // = vec3(vec2(1.0 / texSize.x), 0.0);

varying vec2 scaled;
varying vec3 misc;

void main()
{
	vec3 t = (vec3(floor(scaled.x)) + alpha * vec3(fract(scaled.x))) * texStepX + misc;
	vec4 col1 = texture2D(tex, t.xz);
	vec4 col2 = texture2D(tex, t.yz);

	float scan = scan_c + scan_b * abs(fract(scaled.y) - scan_a);
	gl_FragColor = (col1 + col2) * scan;
}
