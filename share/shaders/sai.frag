// $Id: $

uniform sampler2D tex;

varying vec4 posABCD;
varying vec4 posEL;
varying vec4 posGJ;
varying vec3 scaled;

void swap(inout vec3 a, inout vec3 b)
{
	vec3 t = a; a = b; b = t;
}

bvec2 and(bvec2 a, bvec2 b)
{
	//return a && b;
	return bvec2(vec2(a) * vec2(b));
	//return bvec2((a) * (b));
	//return bvec2(a.x && b.x, a.y && b.y);
}
bvec4 and(bvec4 a, bvec4 b)
{
	//return a && b;
	return bvec4(vec4(a) * vec4(b));
	//return bvec4((a) * (b));
	//return bvec4(a.x && b.x, a.y && b.y, a.z && b.z, a.w && b.w);
}

void main()
{
	vec3 A = texture2D(tex, posABCD.xy).rgb;
	vec3 B = texture2D(tex, posABCD.zy).rgb;
	vec3 C = texture2D(tex, posABCD.xw).rgb;
	vec3 D = texture2D(tex, posABCD.zw).rgb;
	vec3 pp = fract(scaled);

	bvec2 b1 = bvec2(A == D, B == C);
	bvec2 b2 = and(b1, not(b1.yx));
	if (b2.x || b2.y) {
		vec4 pos1 = posEL.xyzw;
		vec4 pos2 = posGJ.xyzw;
		vec2 p = pp.xz;  
		if (b2.y) {
			swap(A, B);
			swap(C, D);
			pos1 = posEL.zyxw;
			pos2 = posGJ.zyxw;
			p = pp.yz;  
		}
		vec3 E = texture2D(tex, pos1.xy).rgb;
		vec3 L = texture2D(tex, pos1.zw).rgb;
		vec3 G = texture2D(tex, pos2.xy).rgb;
		vec3 J = texture2D(tex, pos2.zw).rgb;

		vec2 d = p / 2.0 - 0.25;
		float d2 = p.y - p.x;

		bvec4 b5 = bvec4(A==J, A==E, A==G, A==L);
		bvec4 b7 = and(b5, not(b5.yxwz));
		bvec4 l;
		l.xy = lessThan(vec2(d), vec2(0.0));
		l.zw = not(l.xy);
		bvec4 b8 = and(l.ywzx, b7.xzyw);

		if (b8.x) {
			gl_FragColor.rgb = mix(A, B, -d.y);
		} else if (b8.y) {
			gl_FragColor.rgb = mix(A, C,  d.y);
		} else if (b8.z) {
			gl_FragColor.rgb = mix(A, B,  d.x);
		} else if (b8.w) {
			gl_FragColor.rgb = mix(A, C, -d.x);
		} else if (d2 < 0.0) {
			gl_FragColor.rgb = mix(A, B, -d2);
		} else {
			gl_FragColor.rgb = mix(A, C,  d2);
		}
	} else {
		gl_FragColor.rgb = mix(mix(A, B, pp.x), mix(C, D, pp.x), pp.z);
	}
}
