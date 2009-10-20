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

	vec4 left  = texture2D(tex, posLeftTop.xy);
	vec4 top   = texture2D(tex, posLeftTop.zw);
	vec4 right = texture2D(tex, posRightBot.xy);
	vec4 bot   = texture2D(tex, posRightBot.zw);

	if (dot(left.rgb - right.rgb, top.rgb - bot.rgb) == 0.0 || left.rgb != top.rgb) {
		gl_FragColor = texture2D(tex, gl_TexCoord[0].st);
	} else {
		gl_FragColor = top;
	}
}
