// $Id$

uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler2D offsetTex;

varying vec2 leftTop;
varying vec2 edgePos;
varying vec4 misc;

void main()
{
	float edgeBits = texture2D(edgeTex, edgePos).x;
	//gl_FragColor = vec4(edgeBits, fract(edgeBits * 16.0), fract(edgeBits * 256.0), 1.0);

	// transform (N x N x 4096) to (64N x 64N) texture coords
	float t = 64.0 * edgeBits;
	vec2 xy = vec2(fract(t), floor(t)/64.0);
	vec2 subPixelPos = misc.xy;
	xy += fract(subPixelPos) / 64.0;
	vec2 offset = texture2D(offsetTex, xy).xw;

	// fract not really needed, but it eliminates one MOV instruction
	vec2 texStep2 = fract(misc.zw);
	gl_FragColor = texture2D(colorTex, leftTop + offset * texStep2);
}
