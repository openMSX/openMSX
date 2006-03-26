// $Id$
// Scale2x scaler.

uniform sampler2D tex;

float pi = 4.0 * atan(1.0);

const vec2 texSize = vec2(1024.0, -512.0);

const vec2 texStepH = vec2(1.0 / texSize.x, 0.0);
const vec2 texStepV = vec2(0.0, 1.0 / texSize.y);
const vec4 texStep = vec4(texStepH, texStepV);

void main()
{
	vec2 dir = sin(gl_TexCoord[0].st * texSize * 2.0 * pi);
	vec4 delta = dir.xyxy * texStep;

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
