// $Id$
// TV-like effect where bright pixels are larger than dark pixels.

uniform sampler2D tex;
uniform sampler2D videoTex;

varying vec4 intCoord;
varying vec4 cornerCoord0;
varying vec4 cornerCoord1;

// Pixel separation: lower value means larger pixels.
const vec2 sep = vec2(2.3, 5.7);

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
	float size = max(max(col.r, col.g), col.b);
	return exp2(-dist2s / size) * col;
}

void main()
{
	vec4 distComp = fract(intCoord);
	vec4 distComp2s = distComp * distComp * sep.xyxy;
	vec4 dist2s = distComp2s.xyzw + distComp2s.yzwx;
	gl_FragColor =
		calcCorner(cornerCoord0.xy, cornerCoord1.xy, dist2s.x) +
		calcCorner(cornerCoord0.zy, cornerCoord1.zy, dist2s.y) +
		calcCorner(cornerCoord0.xw, cornerCoord1.xw, dist2s.w) +
		calcCorner(cornerCoord0.zw, cornerCoord1.zw, dist2s.z);
}
