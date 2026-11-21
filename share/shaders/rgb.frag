uniform sampler2D tex;
uniform sampler2D videoTex;
uniform float superMix; // 0: alpha, 1: opaque at 50%, 2: 50/50 mix, 3: video only
uniform float videoGain; // intensity of the external video
uniform float computerLevel; // intensity of the image of the computer over the video
float superAlpha(float a) { a *= (computerLevel > 0.0) ? 1.0 : 0.0; return superMix > 2.5 ? 0.0 : (superMix > 1.5 ? 0.5 : (superMix > 0.5 ? a * 0.5 : a)); }
uniform vec4 cnsts;

varying vec4 scaled;
varying vec2 pos;
varying vec2 videoCoord;

// saturate operations are free on nvidia hardware
vec3 saturate(vec3 x)
{
	return clamp(x, 0.0, 1.0);
}

void shaderMain()
{
	float scan_a    = cnsts.x;
	float scan_b_c2 = cnsts.y; // scan_b * c2
	float scan_c_c2 = cnsts.z; // scan_c * c2
	float c1_2_2    = cnsts.w; // (c1 - c2) / c2

	float scan_c2 = scan_c_c2 + scan_b_c2 * abs(fract(scaled.w) - scan_a);

	const float BIG = 128.0; // big number, actual value is not important
	vec3 m = saturate((-BIG * fract(scaled.zyx)) + vec3(2.0 * BIG / 3.0));

	vec4 col = texture2D(tex, pos);
#if SUPERIMPOSE
	vec4 vid = texture2D(videoTex, videoCoord);
	vid.rgb *= videoGain;
	vec4 p = mix(vid, vec4(col.rgb * computerLevel, col.a), superAlpha(col.a));
#else
	vec4 p = col;
#endif
	vec3 n = p.rgb * scan_c2;
	vec3 s_n = n * c1_2_2 + saturate((n - 1.0) / 2.0);
	gl_FragColor.rgb = n + m * s_n;
	gl_FragColor.a   = p.a;
}

// Without external video the video mix of the NMS 8280 halves the image.
uniform float computerGain;

void main()
{
	shaderMain();
#if !SUPERIMPOSE
	gl_FragColor.rgb *= computerGain;
#endif
}
