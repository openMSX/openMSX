uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler2D offsetTex;
uniform sampler2D weightTex;
uniform sampler2D videoTex;
uniform float superMix; // 0: alpha, 1: opaque at 50%, 2: 50/50 mix, 3: video only
uniform float videoGain; // intensity of the external video
uniform float computerLevel; // intensity of the image of the computer over the video
float superAlpha(float a) { a *= (computerLevel > 0.0) ? 1.0 : 0.0; return superMix > 2.5 ? 0.0 : (superMix > 1.5 ? 0.5 : (superMix > 0.5 ? a * 0.5 : a)); }

varying vec2 mid;
varying vec2 leftTop;
varying vec2 edgePos;
varying vec2 weightPos;
varying vec2 texStep2; // could be uniform
varying vec2 videoCoord;

void shaderMain()
{
	// 12-bit edge information encoded as 64x64 texture-coordinate
	vec2 xy = texture2D(edgeTex, edgePos).ra;

	// extend to (64N x 64N) texture-coordinate
	xy = (floor(64.0 * xy) + fract(weightPos)) / 64.0;
	vec4 offsets = texture2D(offsetTex, xy);
	vec3 weights = texture2D(weightTex, xy).xyz;

	vec4 c5 = texture2D(colorTex, mid);
	vec4 cx = texture2D(colorTex, leftTop + texStep2 * offsets.xy);
	vec4 cy = texture2D(colorTex, leftTop + texStep2 * offsets.zw);

	vec4 col = cx * weights.x + cy * weights.y + c5 * weights.z;

#if SUPERIMPOSE
	vec4 vid = texture2D(videoTex, videoCoord);
	vid.rgb *= videoGain;
	gl_FragColor = mix(vid, vec4(col.rgb * computerLevel, col.a), superAlpha(col.a));
#else
	gl_FragColor = col;
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
