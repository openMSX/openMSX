uniform sampler2D u_tex;

varying float v_color;
varying vec2 v_texCoord;

void main()
{
	gl_FragColor = v_color * texture2D(u_tex, v_texCoord);
}
