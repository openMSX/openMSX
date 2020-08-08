uniform mat4 u_mvpMatrix;
uniform mat3 u_normalMatrix;

attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_texCoord;

varying float v_color;
varying vec2 v_texCoord;

void main()
{
	gl_Position = u_mvpMatrix * vec4(a_position, 1.0);
	v_texCoord = a_texCoord;
	v_color = (u_normalMatrix * a_normal).z;
}
