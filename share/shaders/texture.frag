uniform sampler2D u_tex;
uniform vec4 u_color;

varying vec2 v_texCoord;

void main()
{
	gl_FragColor = texture2D(u_tex, v_texCoord) * u_color;
}
