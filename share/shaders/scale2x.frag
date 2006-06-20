// $Id$
// Scale2x scaler.

uniform sampler2D tex;
varying vec2 texStep; // could be uniform
varying vec2 coord2pi;

void main()
{
	vec4 delta;
	delta.xw = sin(coord2pi) * texStep;
	delta.yz = vec2(0.0);
	
	vec4 posLeftTop  = gl_TexCoord[0].stst - delta;
	vec4 posRightBot = gl_TexCoord[0].stst + delta;

	vec3 left  = texture2D(tex, posLeftTop.xy).rgb;
	vec3 top   = texture2D(tex, posLeftTop.zw).rgb;
	vec3 right = texture2D(tex, posRightBot.xy).rgb;
	vec3 bot   = texture2D(tex, posRightBot.zw).rgb;

	if (dot(left - right, top - bot) == 0.0 || left != top) {
		gl_FragColor = texture2D(tex, gl_TexCoord[0].st);
	} else {
		gl_FragColor = vec4(top, 1.0);
	}
}
