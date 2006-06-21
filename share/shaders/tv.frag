// $Id$
// TV-like effect where bright pixels are larger than dark pixels.

// nVidia offers half-precision floats.
// These defines use full-precision floats instead for other vendors.
#ifndef __GLSL_CG_DATA_TYPES
#define half  float
#define half2 vec2
#define half3 vec3
#define half4 vec4
#endif

uniform sampler2D tex;

varying vec4 intCoord;
varying vec4 cornerCoord;

// Pixel separation: lower value means larger pixels.
const half2 sep = half2(2.3, 5.7);

half4 calcCorner(const vec2 texCoord, const half dist2s)
{
	vec4 col = texture2D(tex, texCoord);
	// TODO: Pixel size could be precomputed in the alpha channel.
	half size = half(max(max(col.r, col.g), col.b));
	return exp2(-dist2s / size) * half4(col);
}

void main()
{
	half4 distComp = half4(fract(intCoord));
	half4 distComp2s = distComp * distComp * sep.xyxy;
	half4 dist2s = distComp2s.xyzw + distComp2s.yzwx;
	gl_FragColor = vec4(
		calcCorner(cornerCoord.xy, dist2s.x) +
		calcCorner(cornerCoord.zy, dist2s.y) +
		calcCorner(cornerCoord.xw, dist2s.w) +
		calcCorner(cornerCoord.zw, dist2s.z));
}
