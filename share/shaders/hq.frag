uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler2D offsetTex;
uniform sampler2D weightTex;
uniform sampler2D videoTex;

varying vec2 mid;
varying vec2 leftTop;
varying vec2 edgePos;
varying vec2 weightPos;
varying vec2 texStep2; // could be uniform
varying vec2 videoCoord;

void main()
{
	// distribute 12-bit 'edge' information over 2 x 6-bit
	vec2 la = texture2D(edgeTex, edgePos).ra; // 4 (LSB) bits in 'r', 8 (MSB) bits in 'a'
	float xx = fract(floor(256.0 * la.y) / 4.0) + (la.x / 4.0); // 6 LSB bits
	float yy = floor(64.0 * la.y) / 64.0;                       // 6 MSB bits
	vec2 xy = vec2(xx, yy);

	xy += fract(weightPos) / 64.0;
	vec4 offsets = texture2D(offsetTex, xy);
	vec3 weights = texture2D(weightTex, xy).xyz;

	vec4 c5 = texture2D(colorTex, mid);
	vec4 cx = texture2D(colorTex, leftTop + texStep2 * offsets.xy);
	vec4 cy = texture2D(colorTex, leftTop + texStep2 * offsets.zw);

	vec4 col = cx * weights.x + cy * weights.y + c5 * weights.z;

#if SUPERIMPOSE
	vec4 vid = texture2D(videoTex, videoCoord);
	gl_FragColor = mix(vid, col, col.a);
#else
	gl_FragColor = col;
#endif
}
