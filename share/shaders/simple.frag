uniform sampler2D tex;
uniform sampler2D videoTex;
uniform float superMix; // 0: alpha, 1: opaque at 50%, 2: 50/50 mix, 3: video only
uniform float videoGain; // intensity of the external video
uniform float computerLevel; // intensity of the image of the computer over the video
float superAlpha(float a) { a *= (computerLevel > 0.0) ? 1.0 : 0.0; return superMix > 2.5 ? 0.0 : (superMix > 1.5 ? 0.5 : (superMix > 0.5 ? a * 0.5 : a)); }
uniform vec4 cnst;
uniform vec3 texStepX; // = vec3(vec2(1.0 / texSize.x), 0.0);

varying vec2 scaled;
varying vec3 misc;
varying vec2 videoCoord;

void shaderMain()
{
	float scan_a = cnst.x;
	float scan_b = cnst.y;
	float scan_c = cnst.z;
	float alpha  = cnst.w;

	vec3 t = (vec3(floor(scaled.x)) + alpha * vec3(fract(scaled.x))) * texStepX + misc;
	vec4 col1 = texture2D(tex, t.xz);
	vec4 col2 = texture2D(tex, t.yz);

	float scan = scan_c + scan_b * abs(fract(scaled.y) - scan_a);
#if SUPERIMPOSE
	vec4 col = (col1 + col2) / 2.0;
	vec4 vid = texture2D(videoTex, videoCoord);
	vid.rgb *= videoGain;
	gl_FragColor = mix(vid, vec4(col.rgb * computerLevel, col.a), superAlpha(col.a)) * scan;
#else
	// optimization: in case of not-superimpose, we moved the division by 2
	// '(col1 + col2) / 2' to the 'scan_b' and 'scan_c' variables.
	gl_FragColor = (col1 + col2) * scan;
#endif
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
