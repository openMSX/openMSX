// $Id$

uniform sampler2D edgeTex;
uniform sampler2D colorTex;
uniform sampler2D linearTex;
uniform sampler2D offsetTex;
uniform sampler2D weightTex;

varying vec2 mid;
varying vec2 leftTop;
varying vec2 edgePos;
varying vec2 weightPos;
varying vec2 texStep2; // could be uniform

void main()
{
	float edgeBits = texture2D(edgeTex, edgePos).y;

	// These are actually 2 times a 3D texture lookup, but on my
	// gfx card (Nvidia Geforce 6200) max texture dimension for 3D
	// textures is more limited (max 512) then for 2D textures (max
	// 4096). We need 4096.
	float linear = texture2D(linearTex, weightPos).x;
	vec4 offsets = texture2D(offsetTex, vec2(linear, edgeBits));
	vec3 weights = texture2D(weightTex, vec2(linear, edgeBits)).xyz;

	vec4 c5 = texture2D(colorTex, mid);
	vec4 cx = texture2D(colorTex, leftTop + texStep2 * offsets.xy);
	vec4 cy = texture2D(colorTex, leftTop + texStep2 * offsets.zw);

	gl_FragColor = cx * weights.x + cy * weights.y + c5 * weights.z;
}
