// $Id$

#include "GLHQScaler.hh"
#include "GLUtil.hh"
#include "HQCommon.hh"
#include "FrameSource.hh"
#include "FileContext.hh"
#include "File.hh"
#include "StringOp.hh"
#include "openmsx.hh"

using std::string;

namespace openmsx {

// TODO apply this transformation on the data file, or even better
// generate the data algorithmically (including this transformation)
static void transform(const byte* in, byte* out, int n, int s)
{
	for (int z = 0; z < 4096; ++z) {
		int z1Offset = s * n * n * z;
		int z2Offset = s * (n * (z % 32) +
		                    n * n * 32 * (z / 32));
		for (int y = 0; y < n; ++y) {
			int y1Offset = s * n      * y;
			int y2Offset = s * n * 32 * y;
			for (int x = 0; x < n; ++x) {
				int offset1 = z1Offset + y1Offset + s * x;
				int offset2 = z2Offset + y2Offset + s * x;
				for (int t = 0; t < s; ++t) {
					out[offset2 + t] = in[offset1 + t];
				}
			}
		}
	}
}

GLHQScaler::GLHQScaler()
{
	VertexShader   vertexShader  ("hq.vert");
	FragmentShader fragmentShader("hq.frag");
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
	byte buffer[4 * 4 * 4 * 4096];
	for (int i = 0; i < 3; ++i) {
		int n = i + 2;
		string offsetsName = "shaders/HQ" + StringOp::toString(n) +
		                     "xOffsets.dat";
		File offsetsFile(context.resolve(offsetsName));
		offsetTexture[i].reset(new Texture(GL_TEXTURE_3D));
		offsetTexture[i]->setWrapMode(false);
		offsetTexture[i]->bind();
		transform(offsetsFile.mmap(), buffer, n, 4);
		glTexImage3D(GL_TEXTURE_3D,      // target
			     0,                  // level
			     GL_RGBA8,           // internal format
			     n * 32,             // width
			     n,                  // height
			     4096 / 32,          // depth
			     0,                  // border
			     GL_RGBA,            // format
			     GL_UNSIGNED_BYTE,   // type
			     buffer);            // data

		string weightsName = "shaders/HQ" + StringOp::toString(n) +
		                     "xWeights.dat";
		File weightsFile(context.resolve(weightsName));
		weightTexture[i].reset(new Texture(GL_TEXTURE_3D));
		weightTexture[i]->setWrapMode(false);
		weightTexture[i]->bind();
		transform(weightsFile.mmap(), buffer, n, 3);
		glTexImage3D(GL_TEXTURE_3D,      // target
			     0,                  // level
			     GL_RGB8,            // internal format
			     n * 32,             // width
			     n,                  // height
			     4096 / 32,          // depth
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
		glUniform1i(scalerProgram->getUniformLocation("offsetTex"), 2);
		glUniform1i(scalerProgram->getUniformLocation("weightTex"), 3);
		glUniform2f(scalerProgram->getUniformLocation("texSize"),
		            320.0f, 2 * 240.0f);
	}
#endif
}

void GLHQScaler::scaleImage(
	ColourTexture& src,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth)
{
	unsigned factorX = dstWidth / srcWidth; // 1 - 4
	unsigned factorY = (dstEndY - dstStartY) / (srcEndY - srcStartY);

	if ((srcWidth == 320) && (factorX > 1) && (factorX == factorY)) {
		assert(src.getHeight() == 2 * 240);
		glActiveTexture(GL_TEXTURE3);
		weightTexture[factorX - 2]->bind();
		glActiveTexture(GL_TEXTURE2);
		offsetTexture[factorX - 2]->bind();
		glActiveTexture(GL_TEXTURE1);
		edgeTexture->bind();
		glActiveTexture(GL_TEXTURE0);
		scalerProgram->activate();
	} else {
		scalerProgram->deactivate();
	}

	GLfloat height = src.getHeight();
	src.drawRect(0.0f,  srcStartY            / height,
	             1.0f, (srcEndY - srcStartY) / height,
	             0, dstStartY, dstWidth, dstEndY - dstStartY);
}

typedef unsigned Pixel;
static void calcInitialEdges(const Pixel* srcPrev, const Pixel* srcCurr,
                             unsigned lineWidth, word* edgeBuf)
{
	unsigned x = 0;
	Pixel c1 = srcPrev[x];
	Pixel c2 = srcCurr[x];
	word pattern = edge(c1, c2) ? (3 << (6 + 4)) : 0;
	for (/* */; x < (lineWidth - 1); ++x) {
		pattern >>= 6;
		Pixel n1 = srcPrev[x + 1];
		Pixel n2 = srcCurr[x + 1];
		if (edge(c1, c2)) pattern |= (1 << (5 + 4));
		if (edge(c1, n2)) pattern |= (1 << (6 + 4));
		if (edge(c2, n1)) pattern |= (1 << (7 + 4));
		edgeBuf[x] = pattern;
		c1 = n1; c2 = n2;
	}
	pattern >>= 6;
	if (edge(c1, c2)) pattern |= (7 << (5 + 4));
	edgeBuf[x] = pattern;
}
void GLHQScaler::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth,
	FrameSource& paintFrame)
{
	if (lineWidth != 320) return;

	word edgeBuf[320 * (240 + 1)];
	Pixel* dummy = 0;
	const Pixel* curr = paintFrame.getLinePtr(srcStartY - 1, lineWidth, dummy);
	const Pixel* next = paintFrame.getLinePtr(srcStartY + 0, lineWidth, dummy);
	calcInitialEdges(curr, next, lineWidth, edgeBuf);

	for (unsigned y = srcStartY; y < srcEndY; ++y) {
		curr = next;
		next = paintFrame.getLinePtr(y + 1, lineWidth, dummy);

		word* edges = &edgeBuf[320 * (y - srcStartY)];
		unsigned pattern = 0;
		unsigned c6 = curr[0];
		unsigned c9 = next[0];
		if (edge(c6, c9))              pattern |= 3 << (6 + 4);
		if (edges[0] & (1 << (0 + 4))) pattern |= 3 << (9 + 4);
		unsigned x = 0;
		for (/* */; x < (lineWidth - 1); ++x) {
			unsigned c5 = c6;
			unsigned c8 = c9;
			c6 = curr[x + 1];
			c9 = next[x + 1];
			pattern = (pattern >> 6) & (0x001F << 4);
			if (edge(c5, c8)) pattern |= 1 << (5 + 4);
			if (edge(c5, c9)) pattern |= 1 << (6 + 4);
			if (edge(c6, c8)) pattern |= 1 << (7 + 4);
			if (edge(c5, c6)) pattern |= 1 << (8 + 4);
			pattern |= ((edges[x] & (1 << (5 + 4))) << 6) |
				   ((edges[x] & (3 << (6 + 4))) << 3);
			edges[x + 320] = pattern | 8;
		}
		pattern = (pattern >> 6) & (0x001F << 4);
		if (edge(c6, c9)) pattern |= 7 << (5 + 4);
		pattern |= ((edges[x] & (1 << (5 + 4))) << 6) |
			   ((edges[x] & (3 << (6 + 4))) << 3);
		edges[x + 320] = pattern | 8;
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
	                &edgeBuf[320]);      // data
}

} // namespace openmsx
