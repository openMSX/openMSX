// $Id$

uniform sampler2D tex;
uniform vec4 cnst;
uniform vec3 texStepX; // = vec3(vec2(1.0 / texSize.x), 0.0);

varying vec2 scaled;
varying vec3 misc;

void main()
{
	float scan_a = cnst.x;
	float scan_b = cnst.y;
	float scan_c = cnst.z;
	float alpha  = cnst.w;

	vec3 t = (vec3(floor(scaled.x)) + alpha * vec3(fract(scaled.x))) * texStepX + misc;
	vec4 col1 = texture2D(tex, t.xz);
	vec4 col2 = texture2D(tex, t.yz);

	float scan = scan_c + scan_b * abs(fract(scaled.y) - scan_a);
	gl_FragColor = (col1 + col2) * scan;
}
