uniform mat4 u_mvpMatrix;
uniform vec3 texSize; // xy = texSize1,  xz = texSize2

attribute vec4 a_position;
attribute vec3 a_texCoord;

varying vec2 texCoord;

void main()
{
	gl_Position = u_mvpMatrix * a_position;
	texCoord = a_texCoord.xy;
}
