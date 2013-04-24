uniform vec3 texSize; // xy = texSize1,  xz = texSize2

varying vec4 intCoord;
varying vec4 cornerCoord0;
varying vec4 cornerCoord1;

void main()
{
	gl_Position = ftransform();
	intCoord.xy = gl_MultiTexCoord0.xy * texSize.xy;
	intCoord.zw = -intCoord.xy;
	cornerCoord0 = vec4(gl_MultiTexCoord0.xy,
	                    gl_MultiTexCoord0.xy + 1.0 / texSize.xy);
	cornerCoord1 = vec4(gl_MultiTexCoord1.xy,
	                    gl_MultiTexCoord1.xy + 1.0 / texSize.xz);
}
