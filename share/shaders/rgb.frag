// $Id: $

uniform sampler2D tex;
uniform vec4 cnsts;

varying vec4 scaled;
varying vec2 pos;

// saturate operations are free on nvidia hardware
vec3 saturate(vec3 x)
{
	return clamp(x, 0.0, 1.0);
}
vec4 saturate(vec4 x)
{
	return clamp(x, 0.0, 1.0);
}

void main()
{
	float scan_a    = cnsts.x; 
	float scan_b_c2 = cnsts.y; // scan_b * c2
	float scan_c_c2 = cnsts.z; // scan_c * c2
	float c1_2_2    = cnsts.w; // (c1 - c2) / c2

	float scan_c2 = scan_c_c2 + scan_b_c2 * abs(fract(scaled.w) - scan_a);

	const float BIG = 128.0; // big number, actual value is not important
	vec3 m = saturate((-BIG * fract(scaled.zyx)) + vec3(2.0 * BIG / 3.0));

	vec4 n = texture2D(tex, pos) * scan_c2;
	vec4 s_n = n * c1_2_2 + saturate((n - 1.0) / 2.0);
	gl_FragColor = n + m.xyzz * s_n;
}
