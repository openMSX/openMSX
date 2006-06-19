// $Id$

varying vec4 cornerCoord;

void main()
{
	gl_Position = ftransform();
	cornerCoord = vec4(gl_MultiTexCoord0.xy - vec2(0.5, 0.5),
	                   gl_MultiTexCoord0.xy + vec2(0.5, 0.5));
}
