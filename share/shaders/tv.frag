// $Id$
// TV-like effect where bright pixels are larger than dark pixels.

uniform sampler2D tex;
uniform sampler2D videoTex;

varying vec4 intCoord;
varying vec4 cornerCoord0;
varying vec4 cornerCoord1;

// Line separation: lower value means larger lines.
const float sep = 4.0;

vec4 calcCorner(const vec2 texCoord0,
                const vec2 texCoord1,
                const float dist2s)
{
	vec4 col0 = texture2D(tex, texCoord0);
#if SUPERIMPOSE
	vec4 vid = texture2D(videoTex, texCoord1);
	vec4 col = mix(vid, col0, col0.a);
#else
	vec4 col = col0;
#endif
	vec4 size = (0.5 + 0.5 * col);
	return exp2(-dist2s / size) * col;
}

void main()
{
	vec4 distComp = fract(intCoord);
	vec2 distComp2s = distComp.yw * distComp.yw * sep;
	gl_FragColor = mix(
		calcCorner(cornerCoord0.zy, cornerCoord1.zy, distComp2s.x) +
		calcCorner(cornerCoord0.zw, cornerCoord1.zw, distComp2s.y),
		calcCorner(cornerCoord0.xy, cornerCoord1.xy, distComp2s.x) +
		calcCorner(cornerCoord0.xw, cornerCoord1.xw, distComp2s.y),
		smoothstep(0.0, 1.0, distComp.z));
}
