// $Id$

uniform sampler2D tex;
uniform sampler2D videoTex;

varying vec2 texCoord;
varying vec2 videoCoord;

void main()
{
	vec4 col = texture2D(tex, texCoord);
	vec4 vid = texture2D(videoTex, videoCoord);
	gl_FragColor = mix(vid, col, col.a);
}
