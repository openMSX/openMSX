// $Id$

// Just a funny effect to play with.

uniform sampler2D tex;

const vec2 texOffset = 0.5 * vec2(320.0 / 1024.0, 240.0 / 512.0);
const float pi = 4.0 * atan(1.0);

// Customize the effect here:
const float warp = 0.4; // low is square, high is warped
const float zoom = 1.2; // low is zoomed in, high is zoomed out

void main()
{
	vec2 screenPos = gl_FragCoord.xy / vec2(640.0, 480.0);
	vec2 round = sin(pi * screenPos.yx);
	vec2 scale = zoom - warp * round;
	//scale *= 4.0;
	vec2 texPos = (gl_TexCoord[0].xy - texOffset) * scale + texOffset;
	//texPos = mod(texPos, vec2(0.5, 0.75 * 0.25));
	vec4 incoming = texture2D(tex, texPos);
	float lum = 1.0; // - 0.5 * sin(2.0 * pi * 240.0 * texPos.y);
	lum *= length(round);
	gl_FragColor = incoming * lum;
}
