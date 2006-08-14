// $Id$

uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler3D offsetTex;
uniform sampler3D weightTex;

varying vec2 mid;
varying vec2 leftTop;
varying vec2 edgePos;
varying vec2 weightPos;
varying vec2 texStep2; // could be uniform

void main()
{
	float edgeBits = texture2D(edgeTex, edgePos).z;
	// transform (N x N x 4096) 3D texture coordinates
	// to (32N x N * 4096/32)
	float x = fract(weightPos.x) * (1.0/32.0) +
	          fract(4096.0/32.0 * (edgeBits - 1.0/8192.0));
	vec4 offsets = texture3D(offsetTex, vec3(x, fract(weightPos.y), edgeBits));
	vec3 weights = texture3D(weightTex, vec3(x, fract(weightPos.y), edgeBits)).xyz;

	vec4 c5 = texture2D(colorTex, mid);
	vec4 cx = texture2D(colorTex, leftTop + texStep2 * offsets.xy);
	vec4 cy = texture2D(colorTex, leftTop + texStep2 * offsets.zw);

	gl_FragColor = cx * weights.x + cy * weights.y + c5 * weights.z;
}
