uniform sampler2D tex;
uniform sampler2D videoTex;
uniform float superMix; // 0: alpha, 1: opaque at 50%, 2: 50/50 mix, 3: video only
uniform float videoGain; // intensity of the external video
uniform float computerLevel; // intensity of the image of the computer over the video
float superAlpha(float a) { a *= (computerLevel > 0.0) ? 1.0 : 0.0; return superMix > 2.5 ? 0.0 : (superMix > 1.5 ? 0.5 : (superMix > 0.5 ? a * 0.5 : a)); }

varying vec2 texCoord;
varying vec2 videoCoord;

void shaderMain()
{
#if SUPERIMPOSE
	vec4 col = texture2D(tex, texCoord);
	vec4 vid = texture2D(videoTex, videoCoord);
	vid.rgb *= videoGain;
	gl_FragColor = mix(vid, vec4(col.rgb * computerLevel, col.a), superAlpha(col.a));
#else
	gl_FragColor = texture2D(tex, texCoord);
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
