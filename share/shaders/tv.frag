// TV-like effect where bright pixels are larger than dark pixels.

uniform sampler2D tex;
uniform sampler2D videoTex;
uniform float superMix; // 0: alpha, 1: opaque at 50%, 2: 50/50 mix, 3: video only
uniform float videoGain; // intensity of the external video
uniform float computerLevel; // intensity of the image of the computer over the video
float superAlpha(float a) { a *= (computerLevel > 0.0) ? 1.0 : 0.0; return superMix > 2.5 ? 0.0 : (superMix > 1.5 ? 0.5 : (superMix > 0.5 ? a * 0.5 : a)); }
uniform float minScanline;
uniform float sizeVariance;

varying vec4 intCoord;
varying vec4 cornerCoord0;
varying vec4 cornerCoord1;

vec4 calcCorner(const vec2 texCoord0,
                const vec2 texCoord1,
                const float dist)
{
	vec4 col0 = texture2D(tex, texCoord0);
#if SUPERIMPOSE
	vec4 vid = texture2D(videoTex, texCoord1);
	vid.rgb *= videoGain;
	vec4 col = mix(vid, vec4(col0.rgb * computerLevel, col0.a), superAlpha(col0.a));
#else
	vec4 col = col0;
#endif
	return col * smoothstep(
		minScanline + sizeVariance * (vec4(1.0) - col),
		vec4(1.0),
		vec4(dist));
}

void shaderMain()
{
	vec4 distComp = fract(intCoord);
	gl_FragColor = mix(
		calcCorner(cornerCoord0.xy, cornerCoord1.xy, distComp.w) +
		calcCorner(cornerCoord0.xw, cornerCoord1.xw, distComp.y),
		calcCorner(cornerCoord0.zy, cornerCoord1.zy, distComp.w) +
		calcCorner(cornerCoord0.zw, cornerCoord1.zw, distComp.y),
		smoothstep(0.0, 1.0, distComp.x));
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
