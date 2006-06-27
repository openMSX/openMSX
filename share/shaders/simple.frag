// $Id$
// Scale2x scaler.

uniform sampler2D tex;
uniform float alpha;
uniform float scan_a;
uniform float scan_b;
uniform float scan_c;

varying vec2 scaled;
varying float y;
varying float texStepX;

void main()
{
	vec2 xi = vec2(floor(scaled.x) + 0.5);
	vec2 xf = vec2(fract(scaled.x));
	vec2 t1 = ((xf - vec2(1.0, 0.0)) * alpha + xi) * texStepX;
	vec3 t2 = vec3(t1, y);
	vec4 col1 = texture2D(tex, t2.xz);
	vec4 col2 = texture2D(tex, t2.yz);
	vec4 col = mix(col1, col2, 0.5);

	float scan = scan_c + scan_b * abs(fract(scaled.y) - scan_a);
	gl_FragColor = col * scan;
}
