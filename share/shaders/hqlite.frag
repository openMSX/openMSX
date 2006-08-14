// $Id$

uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler3D weightTex;

varying vec2 left;
varying vec2 mid;
varying vec2 right;
varying vec2 edgePos;
varying vec2 weightPos;

void main()
{
	float edgeBits = texture2D(edgeTex, edgePos).z;
	// transform (N x N x 4096) 3D texture coordinates
	// to (32N x N * 4096/32)
	float x = fract(weightPos.x) * (1.0/32.0) +
	          fract(4096.0/32.0 * (edgeBits - 1.0/8192.0));
	vec3 weights = texture3D(weightTex, vec3(x, fract(weightPos.y), edgeBits)).xyz;

	vec4 c4 = texture2D(colorTex, left );
	vec4 c5 = texture2D(colorTex, mid  );
	vec4 c6 = texture2D(colorTex, right);

	gl_FragColor = c4 * weights.x + c5 * weights.y + c6 * weights.z;
}
