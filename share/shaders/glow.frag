uniform sampler2D u_tex;
uniform float u_alpha;

varying vec2 v_texCoord;

void main()
{
	gl_FragColor = texture2D(u_tex, v_texCoord) * vec4(1.0, 1.0, 1.0, u_alpha);
}
