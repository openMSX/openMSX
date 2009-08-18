// $Id$

#include "GLHQLiteScaler.hh"
#include "GLUtil.hh"
#include "HQCommon.hh"
#include "FrameSource.hh"
#include "FileContext.hh"
#include "File.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include <cstring>

using std::string;

namespace openmsx {

// TODO apply this transformation on the data file, or even better
// generate the data algorithmically (including this transformation)
static void transform(const byte* in, byte* out, int n)
{
	for (int z = 0; z < 4096; ++z) {
		int z1Offset = 2 * n * n * z;
		int z2Offset = 2 * (n * (z % 64) +
		                    n * n * 64 * (z / 64));
		for (int y = 0; y < n; ++y) {
			int y1Offset = 2 * n      * y;
			int y2Offset = 2 * n * 64 * y;
			for (int x = 0; x < n; ++x) {
				int offset1 = z1Offset + y1Offset + 2 * x;
				int offset2 = z2Offset + y2Offset + 2 * x;
				out[offset2 + 0] = in[offset1 + 0];
				out[offset2 + 1] = in[offset1 + 1];
			}
		}
	}
}

GLHQLiteScaler::GLHQLiteScaler()
{
	VertexShader   vertexShader  ("hqlite.vert");
	FragmentShader fragmentShader("hqlite.frag");
	scalerProgram.reset(new ShaderProgram());
	scalerProgram->attach(vertexShader);
	scalerProgram->attach(fragmentShader);
	scalerProgram->link();

	edgeTexture.reset(new Texture());
	edgeTexture->bind();
	edgeTexture->setWrapMode(false);
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             GL_LUMINANCE16,   // internal format
	             320,              // width
	             240,              // height
	             0,                // border
	             GL_LUMINANCE,     // format
	             GL_UNSIGNED_SHORT,// type
	             NULL);            // data
	edgeBuffer.reset(new PixelBuffer<unsigned short>());
	edgeBuffer->setImage(320, 240);

	SystemFileContext context;
	CommandController* controller = NULL; // ok for SystemFileContext
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	byte buffer[2 * 4 * 4 * 4096];
	for (int i = 0; i < 3; ++i) {
		int n = i + 2;
		string offsetName = StringOp::Builder() <<
			"shaders/HQ" << n << "xLiteOffset.dat";
		File offsetFile(context.resolve(*controller, offsetName));
		offsetTexture[i].reset(new Texture());
		offsetTexture[i]->setWrapMode(false);
		offsetTexture[i]->bind();
		transform(offsetFile.mmap(), buffer, n);
		glTexImage2D(GL_TEXTURE_2D,      // target
			     0,                  // level
			     GL_LUMINANCE8_ALPHA8, // internal format
			     n * 64,             // width
			     n * 64,             // height
			     0,                  // border
			     GL_LUMINANCE_ALPHA, // format
			     GL_UNSIGNED_BYTE,   // type
			     buffer);            // data
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore to default

#ifdef GL_VERSION_2_0
	if (GLEW_VERSION_2_0) {
		scalerProgram->activate();
		glUniform1i(scalerProgram->getUniformLocation("colorTex"),  0);
		glUniform1i(scalerProgram->getUniformLocation("edgeTex"),   1);
		glUniform1i(scalerProgram->getUniformLocation("offsetTex"), 2);
		glUniform2f(scalerProgram->getUniformLocation("texSize"),
		            320.0f, 240.0f);
	}
#endif
}

void GLHQLiteScaler::scaleImage(
	ColorTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	unsigned factorX = dstWidth / srcWidth; // 1 - 4
	unsigned factorY = (dstEndY - dstStartY) / (srcEndY - srcStartY);

	if ((srcWidth == 320) && (factorX > 1) && (factorX == factorY)) {
		src.enableInterpolation();
		glActiveTexture(GL_TEXTURE2);
		offsetTexture[factorX - 2]->bind();
		glActiveTexture(GL_TEXTURE1);
		edgeTexture->bind();
		glActiveTexture(GL_TEXTURE0);
		scalerProgram->activate();
		//scalerProgram->validate();
	} else {
		scalerProgram->deactivate();
	}

	GLfloat height = GLfloat(src.getHeight());
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
	src.disableInterpolation();
}

typedef unsigned Pixel;
void GLHQLiteScaler::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth,
	FrameSource& paintFrame)
{
	if (lineWidth != 320) return;

	unsigned tmpBuf2[320 / 2];
	#ifndef NDEBUG
	// Avoid UMR. In optimized mode we don't care.
	memset(tmpBuf2, 0, (320 / 2) * sizeof(unsigned));
	#endif

	const Pixel* curr = paintFrame.getLinePtr<Pixel>(srcStartY - 1, lineWidth);
	const Pixel* next = paintFrame.getLinePtr<Pixel>(srcStartY + 0, lineWidth);
	calcEdgesGL(curr, next, tmpBuf2, EdgeHQLite());

	edgeBuffer->bind();
	unsigned short* mapped = edgeBuffer->mapWrite();
	if (mapped) {
		for (unsigned y = srcStartY; y < srcEndY; ++y) {
			curr = next;
			next = paintFrame.getLinePtr<Pixel>(y + 1, lineWidth);
			calcEdgesGL(curr, next, tmpBuf2, EdgeHQLite());
			memcpy(mapped + 320 * y, tmpBuf2, 320 * sizeof(unsigned short));
		}
		edgeBuffer->unmap();

		edgeTexture->bind();
		glTexSubImage2D(GL_TEXTURE_2D,       // target
		                0,                   // level
		                0,                   // offset x
		                srcStartY,           // offset y
		                lineWidth,           // width
		                srcEndY - srcStartY, // height
		                GL_LUMINANCE,        // format
		                GL_UNSIGNED_SHORT,   // type
		                edgeBuffer->getOffset(0, srcStartY)); // data
	}
	edgeBuffer->unbind();
}

} // namespace openmsx
