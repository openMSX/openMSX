#include "GLHQScaler.hh"
#include "GLUtil.hh"
#include "HQCommon.hh"
#include "FrameSource.hh"
#include "FileContext.hh"
#include "File.hh"
#include "StringOp.hh"
#include "openmsx.hh"
#include "memory.hh"
#include <cstring>

using std::string;

namespace openmsx {

GLHQScaler::GLHQScaler()
{
	for (int i = 0; i < 2; ++i) {
		string header = string("#define SUPERIMPOSE ")
		              + char('0' + i) + '\n';
		VertexShader   vertexShader  (header, "hq.vert");
		FragmentShader fragmentShader(header, "hq.frag");
		scalerProgram[i].attach(vertexShader);
		scalerProgram[i].attach(fragmentShader);
		scalerProgram[i].link();
#ifdef GL_VERSION_2_0
		if (GLEW_VERSION_2_0) {
			scalerProgram[i].activate();
			glUniform1i(scalerProgram[i].getUniformLocation("colorTex"),  0);
			if (i == 1) {
				glUniform1i(scalerProgram[i].getUniformLocation("videoTex"),   1);
			}
			glUniform1i(scalerProgram[i].getUniformLocation("edgeTex"),   2);
			glUniform1i(scalerProgram[i].getUniformLocation("offsetTex"), 3);
			glUniform1i(scalerProgram[i].getUniformLocation("weightTex"), 4);
			glUniform2f(scalerProgram[i].getUniformLocation("texSize"),
				    320.0f, 2 * 240.0f);
		}
#endif
	}

	edgeTexture.bind();
	edgeTexture.setWrapMode(false);
	glTexImage2D(GL_TEXTURE_2D,    // target
	             0,                // level
	             GL_LUMINANCE16,   // internal format
	             320,              // width
	             240,              // height
	             0,                // border
	             GL_LUMINANCE,     // format
	             GL_UNSIGNED_SHORT,// type
	             nullptr);         // data
	edgeBuffer.setImage(320, 240);

	SystemFileContext context;
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	for (int i = 0; i < 3; ++i) {
		int n = i + 2;
		string offsetsName = StringOp::Builder() <<
			"shaders/HQ" << n << "xOffsets.dat";
		File offsetsFile(context.resolve(offsetsName));
		offsetTexture[i].setWrapMode(false);
		offsetTexture[i].bind();
		size_t size; // dummy
		glTexImage2D(GL_TEXTURE_2D,       // target
		             0,                   // level
		             GL_RGBA8,            // internal format
		             n * 64,              // width
		             n * 64,              // height
		             0,                   // border
		             GL_RGBA,             // format
		             GL_UNSIGNED_BYTE,    // type
		             offsetsFile.mmap(size));// data

		string weightsName = StringOp::Builder() <<
			"shaders/HQ" << n << "xWeights.dat";
		File weightsFile(context.resolve(weightsName));
		weightTexture[i].setWrapMode(false);
		weightTexture[i].bind();
		glTexImage2D(GL_TEXTURE_2D,       // target
		             0,                   // level
		             GL_RGB8,             // internal format
		             n * 64,              // width
		             n * 64,              // height
		             0,                   // border
		             GL_RGB,              // format
		             GL_UNSIGNED_BYTE,    // type
		             weightsFile.mmap(size));// data
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // restore to default
}

void GLHQScaler::scaleImage(
	ColorTexture& src, ColorTexture* superImpose,
	unsigned srcStartY, unsigned srcEndY, unsigned srcWidth,
	unsigned dstStartY, unsigned dstEndY, unsigned dstWidth,
	unsigned logSrcHeight)
{
	unsigned factorX = dstWidth / srcWidth; // 1 - 4
	unsigned factorY = (dstEndY - dstStartY) / (srcEndY - srcStartY);

	auto& prog = scalerProgram[superImpose ? 1 : 0];
	if ((srcWidth == 320) && (factorX > 1) && (factorX == factorY)) {
		assert(src.getHeight() == 2 * 240);
		glActiveTexture(GL_TEXTURE4);
		weightTexture[factorX - 2].bind();
		glActiveTexture(GL_TEXTURE3);
		offsetTexture[factorX - 2].bind();
		glActiveTexture(GL_TEXTURE2);
		edgeTexture.bind();
		if (superImpose) {
			glActiveTexture(GL_TEXTURE1);
			superImpose->bind();
		}
		glActiveTexture(GL_TEXTURE0);
		prog.activate();
		drawMultiTex(src, srcStartY, srcEndY, src.getHeight(), logSrcHeight,
		             dstStartY, dstEndY, dstWidth);
	} else {
		GLScaler::scaleImage(src, superImpose,
		                     srcStartY, srcEndY, srcWidth,
		                     dstStartY, dstEndY, dstWidth,
		                     logSrcHeight);
	}
}

typedef unsigned Pixel;
void GLHQScaler::uploadBlock(
	unsigned srcStartY, unsigned srcEndY, unsigned lineWidth,
	FrameSource& paintFrame)
{
	if (lineWidth != 320) return;

	unsigned tmpBuf2[320 / 2];
	#ifndef NDEBUG
	// Avoid UMR. In optimized mode we don't care.
	memset(tmpBuf2, 0, sizeof(tmpBuf2));
	#endif

	EdgeHQ edgeOp(0, 8, 16);
	const Pixel* curr = paintFrame.getLinePtr<Pixel>(srcStartY - 1, lineWidth);
	const Pixel* next = paintFrame.getLinePtr<Pixel>(srcStartY + 0, lineWidth);
	calcEdgesGL(curr, next, tmpBuf2, edgeOp);

	edgeBuffer.bind();
	if (unsigned short* mapped = edgeBuffer.mapWrite()) {
		for (unsigned y = srcStartY; y < srcEndY; ++y) {
			curr = next;
			next = paintFrame.getLinePtr<Pixel>(y + 1, lineWidth);
			calcEdgesGL(curr, next, tmpBuf2, edgeOp);
			memcpy(mapped + 320 * y, tmpBuf2, 320 * sizeof(unsigned short));
		}
		edgeBuffer.unmap();

		edgeTexture.bind();
		glTexSubImage2D(GL_TEXTURE_2D,       // target
		                0,                   // level
		                0,                   // offset x
		                srcStartY,           // offset y
		                lineWidth,           // width
		                srcEndY - srcStartY, // height
		                GL_LUMINANCE,        // format
		                GL_UNSIGNED_SHORT,   // type
		                edgeBuffer.getOffset(0, srcStartY)); // data
	}
	edgeBuffer.unbind();
}

} // namespace openmsx
