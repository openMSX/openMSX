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
	// 12-bit edge information encoded as 64x64 texture-coordinate
	vec2 xy = texture2D(edgeTex, edgePos).ra;

	// extend to (64N x 64N) texture-coordinate
	vec2 subPixelPos = misc.xy;
	xy = (floor(64.0 * xy) + fract(subPixelPos)) / 64.0;
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
