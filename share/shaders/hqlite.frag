uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler2D offsetTex;
uniform sampler2D videoTex;

varying vec2 leftTop;
varying vec2 edgePos;
varying vec4 misc;
varying vec2 videoCoord;

void main()
{
	// distribute 12-bit 'edge' information over 2 x 6-bit
	vec2 la = texture2D(edgeTex, edgePos).ra; // 4 (LSB) bits in 'r', 8 (MSB) bits in 'a'
	float xx = fract(floor(256.0 * la.y) / 4.0) + (la.x / 4.0); // 6 LSB bits
	float yy = floor(64.0 * la.y) / 64.0;                       // 6 MSB bits
	vec2 xy = vec2(xx, yy);

	vec2 subPixelPos = misc.xy;
	xy += fract(subPixelPos) / 64.0;
	vec2 offset = texture2D(offsetTex, xy).xw;

	// fract not really needed, but it eliminates one MOV instruction
	vec2 texStep2 = fract(misc.zw);
	vec4 col = texture2D(colorTex, leftTop + offset * texStep2);

#if SUPERIMPOSE
	vec4 vid = texture2D(videoTex, videoCoord);
	gl_FragColor = mix(vid, col, col.a);
#else
	gl_FragColor = col;
#endif
}
