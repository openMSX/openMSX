// $Id$

#include "GLHQLiteScaler.hh"
#include "GLUtil.hh"
#include "FrameSource.hh"
#include "FileContext.hh"
#include "File.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include "build-info.hh"

using std::string;

namespace openmsx {

// TODO apply this transformation on the data file, or even better
// generate the data algorithmically (including this transformation)
static void transform(const byte* in, byte* out, int n)
{
	for (int z = 0; z < 4096; ++z) {
		int z1Offset = 3 * n * n * z;
		int z2Offset = 3 * (n * (z % 64) +
		                    n * n * 64 * (z / 64));
		for (int y = 0; y < n; ++y) {
			int y1Offset = 3 * n      * y;
			int y2Offset = 3 * n * 64 * y;
			for (int x = 0; x < n; ++x) {
				int offset1 = z1Offset + y1Offset + 3 * x;
				int offset2 = z2Offset + y2Offset + 3 * x;
				out[offset2 + 0] = in[offset1 + 0];
				out[offset2 + 1] = in[offset1 + 1];
				out[offset2 + 2] = in[offset1 + 2];
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

	SystemFileContext context;
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	byte buffer[3 * 4 * 4 * 4096];
	for (int i = 0; i < 3; ++i) {
		int n = i + 2;
		string weightsName = "shaders/HQ" + StringOp::toString(n) +
		                     "xLiteWeights.dat";
		File weightsFile(context.resolve(weightsName));
		weightTexture[i].reset(new Texture());
		weightTexture[i]->setWrapMode(false);
		weightTexture[i]->bind();
		transform(weightsFile.mmap(), buffer, n);
		glTexImage2D(GL_TEXTURE_2D,      // target
			     0,                  // level
			     GL_RGB8,            // internal format
			     n * 64,             // width
			     n * 64,             // height
			     0,                  // border
			     GL_RGB,             // format
			     GL_UNSIGNED_BYTE,   // type
			     buffer);            // data
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore to default

#ifdef GL_VERSION_2_0
	if (GLEW_VERSION_2_0) {
		scalerProgram->activate();
		glUniform1i(scalerProgram->getUniformLocation("colorTex"),  0);
		glUniform1i(scalerProgram->getUniformLocation("edgeTex"),   1);
		glUniform1i(scalerProgram->getUniformLocation("weightTex"), 2);
		glUniform2f(scalerProgram->getUniformLocation("texSize"),
		            320.0f, 240.0f);
	}
#endif
}

void GLHQLiteScaler::scaleImage(
	ColourTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	unsigned factorX = dstWidth / srcWidth; // 1 - 4
	unsigned factorY = (dstEndY - dstStartY) / (srcEndY - srcStartY);

	if ((srcWidth == 320) && (factorX > 1) && (factorX == factorY)) {
		glActiveTexture(GL_TEXTURE2);
		weightTexture[factorX - 2]->bind();
		glActiveTexture(GL_TEXTURE1);
		edgeTexture->bind();
		glActiveTexture(GL_TEXTURE0);
		scalerProgram->activate();
		//scalerProgram->validate();
	} else {
		scalerProgram->deactivate();
	}

	GLfloat height = src.getHeight();
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
}

typedef unsigned Pixel;
static void calcInitialEdgesGL(const Pixel* curr, const Pixel* next,
                               unsigned* edges2)
{
	if (OPENMSX_BIGENDIAN) {
		Pixel c5 = curr[0];
		Pixel c8 = next[0];
		unsigned pattern = 0;
		if (c5 != c8) pattern |= 0x1400;

		for (unsigned xx = 0; xx < (320 - 2) / 2; ++xx) {
			pattern = (pattern << (16 + 1)) & 0xA8000000;

			if (c5 != c8) pattern |= 0x02000000;
			Pixel c6 = curr[2 * xx + 1];
			if (c6 != c8) pattern |= 0x10002000;
			Pixel c9 = next[2 * xx + 1];
			if (c5 != c9) pattern |= 0x04000800;

			if (c6 != c9) pattern |= 0x0200;
			c5 = curr[2 * xx + 2];
			if (c5 != c9) pattern |= 0x1000;
			c8 = next[2 * xx + 2];
			if (c6 != c8) pattern |= 0x0400;

			edges2[xx + 320 / 2] = pattern;
		}

		pattern = (pattern << (16 + 1)) & 0xA8000000;

		if (c5 != c8) pattern |= 0x02000000;
		Pixel c6 = curr[319];
		if (c6 != c8) pattern |= 0x10002000;
		Pixel c9 = next[319];
		if (c5 != c9) pattern |= 0x04000800;

		if (c6 != c9) pattern |= 0x1600;

		edges2[159 + 320 / 2] = pattern;
	} else {
		Pixel c5 = curr[0];
		Pixel c8 = next[0];
		unsigned pattern = 0;
		if (c5 != c8) pattern |= 0x14000000;

		for (unsigned xx = 0; xx < (320 - 2) / 2; ++xx) {
			pattern = (pattern >> (16 - 1)) & 0xA800;

			if (c5 != c8) pattern |= 0x0200;
			Pixel c6 = curr[2 * xx + 1];
			if (c6 != c8) pattern |= 0x20001000;
			Pixel c9 = next[2 * xx + 1];
			if (c5 != c9) pattern |= 0x08000400;

			if (c6 != c9) pattern |= 0x02000000;
			c5 = curr[2 * xx + 2];
			if (c5 != c9) pattern |= 0x10000000;
			c8 = next[2 * xx + 2];
			if (c6 != c8) pattern |= 0x04000000;

			edges2[xx + 320 / 2] = pattern;
		}

		pattern = (pattern >> (16 - 1)) & 0xA800;

		if (c5 != c8) pattern |= 0x0200;
		Pixel c6 = curr[319];
		if (c6 != c8) pattern |= 0x20001000;
		Pixel c9 = next[319];
		if (c5 != c9) pattern |= 0x08000400;

		if (c6 != c9) pattern |= 0x16000000;

		edges2[159 + 320 / 2] = pattern;
	}
}

void GLHQLiteScaler::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth,
	FrameSource& paintFrame)
{
	if (lineWidth != 320) return;

	unsigned edgeBuf2[(320 / 2) * (240 + 1)];
	Pixel* dummy = 0;
	const Pixel* curr = paintFrame.getLinePtr(srcStartY - 1, lineWidth, dummy);
	const Pixel* next = paintFrame.getLinePtr(srcStartY + 0, lineWidth, dummy);
	calcInitialEdgesGL(curr, next, edgeBuf2);

	for (unsigned y = srcStartY; y < srcEndY; ++y) {
		curr = next;
		next = paintFrame.getLinePtr(y + 1, lineWidth, dummy);

		unsigned* edges2 = &edgeBuf2[(320 / 2) * (y - srcStartY)];
		if (OPENMSX_BIGENDIAN) {
			unsigned pattern = 0;
			Pixel c5 = curr[0];
			Pixel c8 = next[0];
			if (c5 != c8) pattern |= 0x1400;

			for (unsigned xx = 0; xx < (320 - 2) / 2; ++xx) {
				pattern = (pattern << (16 + 1)) & 0xA8000000;
				pattern |= ((edges2[xx] >> 5) & 0x01F001F0);

				if (c5 != c8) pattern |= 0x02000000;
				Pixel c6 = curr[2 * xx + 1];
				if (c6 != c8) pattern |= 0x10002000;
				if (c5 != c6) pattern |= 0x40008000;
				Pixel c9 = next[2 * xx + 1];
				if (c5 != c9) pattern |= 0x04000800;

				if (c6 != c9) pattern |= 0x0200;
				c5 = curr[2 * xx + 2];
				if (c5 != c9) pattern |= 0x1000;
				if (c6 != c5) pattern |= 0x4000;
				c8 = next[2 * xx + 2];
				if (c6 != c8) pattern |= 0x0400;

				edges2[xx + 320 / 2] = pattern;
			}

			pattern = (pattern << (16 + 1)) & 0xA8000000;
			pattern |= ((edges2[159] >> 5) & 0x01F001F0);

			if (c5 != c8) pattern |= 0x02000000;
			Pixel c6 = curr[319];
			if (c6 != c8) pattern |= 0x10002000;
			if (c5 != c6) pattern |= 0x40008000;
			Pixel c9 = next[319];
			if (c5 != c9) pattern |= 0x04000800;

			if (c6 != c9) pattern |= 0x1600;

			edges2[159 + 320 / 2] = pattern;
		} else {
			unsigned pattern = 0;
			Pixel c5 = curr[0];
			Pixel c8 = next[0];
			if (c5 != c8) pattern |= 0x14000000;

			for (unsigned xx = 0; xx < (320 - 2) / 2; ++xx) {
				pattern = (pattern >> (16 -1)) & 0xA800;
				pattern |= ((edges2[xx] >> 5) & 0x01F001F0);

				if (c5 != c8) pattern |= 0x0200;
				Pixel c6 = curr[2 * xx + 1];
				if (c6 != c8) pattern |= 0x20001000;
				if (c5 != c6) pattern |= 0x80004000;
				Pixel c9 = next[2 * xx + 1];
				if (c5 != c9) pattern |= 0x08000400;

				if (c6 != c9) pattern |= 0x02000000;
				c5 = curr[2 * xx + 2];
				if (c5 != c9) pattern |= 0x10000000;
				if (c6 != c5) pattern |= 0x40000000;
				c8 = next[2 * xx + 2];
				if (c6 != c8) pattern |= 0x04000000;

				edges2[xx + 320 / 2] = pattern;
			}

			pattern = (pattern >> (16 -1)) & 0xA800;
			pattern |= ((edges2[159] >> 5) & 0x01F001F0);

			if (c5 != c8) pattern |= 0x0200;
			Pixel c6 = curr[319];
			if (c6 != c8) pattern |= 0x20001000;
			if (c5 != c6) pattern |= 0x80004000;
			Pixel c9 = next[319];
			if (c5 != c9) pattern |= 0x08000400;

			if (c6 != c9) pattern |= 0x16000000;

			edges2[159 + 320 / 2] = pattern;
		}
	}

	edgeTexture->bind();
	glTexSubImage2D(GL_TEXTURE_2D,       // target
	                0,                   // level
	                0,                   // offset x
	                srcStartY,           // offset y
	                lineWidth,           // width
	                srcEndY - srcStartY, // height
	                GL_LUMINANCE,        // format
	                GL_UNSIGNED_SHORT,   // type
	                &edgeBuf2[320 / 2]);  // data
}

} // namespace openmsx
