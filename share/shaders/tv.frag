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
uniform sampler2D videoTex;

varying vec4 intCoord;
varying vec4 cornerCoord0;
varying vec4 cornerCoord1;

// Pixel separation: lower value means larger pixels.
const half2 sep = half2(2.3, 5.7);

half4 calcCorner(const vec2 texCoord0,
                 const vec2 texCoord1,
                 const half dist2s)
{
	vec4 col0 = texture2D(tex, texCoord0);
#if SUPERIMPOSE
	vec4 vid = texture2D(videoTex, texCoord1);
	vec4 col = mix(vid, col0, col0.a);
#else
	vec4 col = col0;
#endif
	half size = half(max(max(col.r, col.g), col.b));
	return exp2(-dist2s / size) * half4(col);
}

void main()
{
	half4 distComp = half4(fract(intCoord));
	half4 distComp2s = distComp * distComp * sep.xyxy;
	half4 dist2s = distComp2s.xyzw + distComp2s.yzwx;
	gl_FragColor = vec4(
		calcCorner(cornerCoord0.xy, cornerCoord1.xy, dist2s.x) +
		calcCorner(cornerCoord0.zy, cornerCoord1.zy, dist2s.y) +
		calcCorner(cornerCoord0.xw, cornerCoord1.xw, dist2s.w) +
		calcCorner(cornerCoord0.zw, cornerCoord1.zw, dist2s.z));
}
