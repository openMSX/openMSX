// $Id$
// Scale2x scaler.

uniform sampler2DRect tex;

float pi = 4.0 * atan(1.0);
float pi2 = 2.0 * pi;

void main()
{
	vec4 delta;
	delta.xw = sin(gl_TexCoord[0].st * pi2);
	delta.yz = vec2(0.0, 0.0);

	vec4 posLeftTop  = gl_TexCoord[0].stst - delta;
	vec4 posRightBot = gl_TexCoord[0].stst + delta;

	vec3 left  = texture2DRect(tex, posLeftTop.xy).rgb;
	vec3 top   = texture2DRect(tex, posLeftTop.zw).rgb;
	vec3 right = texture2DRect(tex, posRightBot.xy).rgb;
	vec3 bot   = texture2DRect(tex, posRightBot.zw).rgb;

	if (dot(left - right, top - bot) == 0.0 || left != top) {
		gl_FragColor = texture2DRect(tex, gl_TexCoord[0].st);
	} else {
		gl_FragColor = vec4(top, 1.0);
	}
}
