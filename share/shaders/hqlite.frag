// $Id$

uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler2D linearTex;
uniform sampler2D weightTex;

varying vec2 left;
varying vec2 mid;
varying vec2 right;
varying vec2 edgePos;
varying vec2 weightPos;

void main()
{
	float edgeBits = texture2D(edgeTex, edgePos).y;

	// These 2 together are actually a 3D texture lookup, but on my
	// gfx card (Nvidia Geforce 6200) max texture dimension for 3D
	// textures is more limited (max 512) then for 2D textures (max
	// 4096). We need 4096.
	float linear = texture2D(linearTex, weightPos).x;
	vec3 weights = texture2D(weightTex, vec2(linear, edgeBits)).xyz;

	vec4 c4 = texture2D(colorTex, left );
	vec4 c5 = texture2D(colorTex, mid  );
	vec4 c6 = texture2D(colorTex, right);

	gl_FragColor = c4 * weights.x + c5 * weights.y + c6 * weights.z;
}
