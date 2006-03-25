// $Id$

// Just a funny effect to play with.

uniform sampler2D tex;

const vec2 texSize = vec2(1024.0, 512.0);
const vec2 displaySize = vec2(320.0, 240.0);
const vec2 texOffset = 0.5 * displaySize / texSize;
float pi = 4.0 * atan(1.0);

// Customize the effect here:
const float warp = 0.4; // low is square, high is warped
const float zoom = 1.2; // low is zoomed in, high is zoomed out

void main()
{
	vec2 round = sin(pi * gl_TexCoord[0].ts * (texSize / displaySize).yx);
	vec2 scale = zoom - warp * round;
	//scale *= 4.0;
	vec2 texPos = (gl_TexCoord[0].st - texOffset) * scale + texOffset;
	//texPos = mod(texPos, vec2(0.5, 0.75 * 0.25));
	vec4 incoming = texture2D(tex, texPos);
	float lum = 1.0; // - 0.5 * sin(2.0 * pi * 240.0 * texPos.y);
	lum *= length(round);
	gl_FragColor = incoming * lum;
}
