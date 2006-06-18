// $Id$

varying vec4 cornerCoord;

void main()
{
	gl_Position = ftransform();
	cornerCoord = vec4(gl_MultiTexCoord0.xy,
	                   gl_MultiTexCoord0.xy + vec2(1.0, 1.0));
}
