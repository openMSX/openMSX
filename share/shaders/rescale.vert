uniform mat4 u_mvpMatrix;

attribute vec4 a_position;
attribute vec2 a_texCoord;

varying vec2 texCoord;

void main()
{
	gl_Position = u_mvpMatrix * a_position;
	texCoord = a_texCoord;
}
