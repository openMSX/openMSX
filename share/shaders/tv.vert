// $Id$

uniform vec2 texSize;

varying vec4 intCoord;
varying vec4 cornerCoord;

void main()
{
	gl_Position = ftransform();
	intCoord.xy = gl_MultiTexCoord0.xy * texSize;
	intCoord.zw = -intCoord.xy;
	cornerCoord = vec4(gl_MultiTexCoord0.xy,
	                   gl_MultiTexCoord0.xy + 1.0 / texSize);
}
