uniform sampler2D tex;
uniform sampler2D videoTex;
uniform vec4 cnst;
uniform vec3 texStepX; // = vec3(vec2(1.0 / texSize.x), 0.0);

varying vec2 scaled;
varying vec3 misc;
varying vec2 videoCoord;

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
#if SUPERIMPOSE
	vec4 col = (col1 + col2) / 2.0;
	vec4 vid = texture2D(videoTex, videoCoord);
	gl_FragColor = mix(vid, col, col.a) * scan;
#else
	// optimization: in case of not-superimpose, we moved the division by 2
	// '(col1 + col2) / 2' to the 'scan_b' and 'scan_c' variables.
	gl_FragColor = (col1 + col2) * scan;
#endif
}
