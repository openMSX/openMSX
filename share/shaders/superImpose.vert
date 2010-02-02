// $Id$

varying vec2 texCoord;
varying vec2 videoCoord;

void main()
{
	gl_Position = ftransform();
	texCoord = gl_MultiTexCoord0.st;
	videoCoord = gl_MultiTexCoord1.st;
}
