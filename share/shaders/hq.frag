// $Id$

uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler2D offsetTex;
uniform sampler2D weightTex;

varying vec2 mid;
varying vec2 leftTop;
varying vec2 edgePos;
varying vec2 weightPos;
varying vec2 texStep2; // could be uniform

void main()
{
	float edgeBits = texture2D(edgeTex, edgePos).z;
	
	// transform (N x N x 4096) to (64N x 64N) texture coords
	float t = 64.0 * edgeBits - 64.0/8192.0;
	vec2 xy = vec2(fract(t), floor(t)/64.0);
	xy += fract(weightPos) / 64.0;
	vec4 offsets = texture2D(offsetTex, xy);
	vec3 weights = texture2D(weightTex, xy).xyz;

	vec4 c5 = texture2D(colorTex, mid);
	vec4 cx = texture2D(colorTex, leftTop + texStep2 * offsets.xy);
	vec4 cy = texture2D(colorTex, leftTop + texStep2 * offsets.zw);

	gl_FragColor = cx * weights.x + cy * weights.y + c5 * weights.z;
}
