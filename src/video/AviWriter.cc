// $Id$

// Code based on DOSBox-0.65

#include "AviWriter.hh"
#include "ZMBVEncoder.hh"
#include "Filename.hh"
#include "CommandException.hh"
#include "build-info.hh"
#include "Version.hh"
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cassert>

namespace openmsx {

static const unsigned AVI_HEADER_SIZE = 500;

static inline void writeLE2(unsigned char* p, unsigned x)
{
	p[0] = (x >> 0) & 255;
	p[1] = (x >> 8) & 255;
}
static inline void writeLE4(unsigned char* p, unsigned x)
{
	p[0] = (x >>  0) & 255;
	p[1] = (x >>  8) & 255;
	p[2] = (x >> 16) & 255;
	p[3] = (x >> 24) & 255;
}

AviWriter::AviWriter(const Filename& filename, unsigned width_,
                     unsigned height_, unsigned bpp, unsigned freq_)
	: fps(50.0)
	, width(width_)
	, height(height_)
	, audiorate(freq_)
{
	std::string resolved = filename.getResolved();
	file = fopen(resolved.c_str(), "wb");
	if (!file) {
		throw CommandException("Couldn't open " + resolved +
		                       " for writing.");
	}
	char dummy[AVI_HEADER_SIZE];
	memset(dummy, 0, AVI_HEADER_SIZE);
	fwrite(dummy, 1, AVI_HEADER_SIZE, file);

	codec.reset(new ZMBVEncoder(width, height, bpp));
	index.resize(8);

	frames = 0;
	written = 0;
	audiowritten = 0;
}

AviWriter::~AviWriter()
{
	unsigned char avi_header[AVI_HEADER_SIZE];
	memset(&avi_header, 0, AVI_HEADER_SIZE);
	unsigned header_pos = 0;

#define AVIOUT4(_S_) memcpy(&avi_header[header_pos],_S_,4);header_pos+=4;
#define AVIOUTw(_S_) writeLE2(&avi_header[header_pos], _S_);header_pos+=2;
#define AVIOUTd(_S_) writeLE4(&avi_header[header_pos], _S_);header_pos+=4;
#define AVIOUTs(_S_) memcpy(&avi_header[header_pos],_S_,strlen(_S_)+1);header_pos+=(strlen(_S_)+1 + 1) & ~1;

	bool hasAudio = audiorate != 0;

	// write avi header
	AVIOUT4("RIFF");                    // Riff header
	AVIOUTd(AVI_HEADER_SIZE + written - 8 + index.size());
	AVIOUT4("AVI ");
	AVIOUT4("LIST");                    // List header
	unsigned main_list = header_pos;
	AVIOUTd(0);                         // size of list, filled in later
	AVIOUT4("hdrl");

	AVIOUT4("avih");
	AVIOUTd(56);                        // # of bytes to follow
	AVIOUTd(unsigned(1000000 / fps));   // Microseconds per frame
	AVIOUTd(0);
	AVIOUTd(0);                         // PaddingGranularity (whatever that might be)
	AVIOUTd(0x110);                     // Flags,0x10 has index, 0x100 interleaved
	AVIOUTd(frames);                    // TotalFrames
	AVIOUTd(0);                         // InitialFrames
	AVIOUTd(hasAudio? 2 : 1);           // Stream count
	AVIOUTd(0);                         // SuggestedBufferSize
	AVIOUTd(width);                     // Width
	AVIOUTd(height);                    // Height
	AVIOUTd(0);                         // TimeScale:  Unit used to measure time
	AVIOUTd(0);                         // DataRate:   Data rate of playback
	AVIOUTd(0);                         // StartTime:  Starting time of AVI data
	AVIOUTd(0);                         // DataLength: Size of AVI data chunk

	// Video stream list
	AVIOUT4("LIST");
	AVIOUTd(4 + 8 + 56 + 8 + 40);       // Size of the list
	AVIOUT4("strl");
	// video stream header
	AVIOUT4("strh");
	AVIOUTd(56);                        // # of bytes to follow
	AVIOUT4("vids");                    // Type
	AVIOUT4(ZMBVEncoder::CODEC_4CC);    // Handler
	AVIOUTd(0);                         // Flags
	AVIOUTd(0);                         // Reserved, MS says: wPriority, wLanguage
	AVIOUTd(0);                         // InitialFrames
	AVIOUTd(1000000);                   // Scale
	AVIOUTd(unsigned(1000000 * fps));   // Rate: Rate/Scale == samples/second
	AVIOUTd(0);                         // Start
	AVIOUTd(frames);                    // Length
	AVIOUTd(0);                         // SuggestedBufferSize
	AVIOUTd(~0);                        // Quality
	AVIOUTd(0);                         // SampleSize
	AVIOUTd(0);                         // Frame
	AVIOUTd(0);                         // Frame
	// The video stream format
	AVIOUT4("strf");
	AVIOUTd(40);                        // # of bytes to follow
	AVIOUTd(40);                        // Size
	AVIOUTd(width);                     // Width
	AVIOUTd(height);                    // Height
	AVIOUTd(0);
	AVIOUT4(ZMBVEncoder::CODEC_4CC);    // Compression
	AVIOUTd(width * height * 4);        // SizeImage (in bytes?)
	AVIOUTd(0);                         // XPelsPerMeter
	AVIOUTd(0);                         // YPelsPerMeter
	AVIOUTd(0);                         // ClrUsed: Number of colors used
	AVIOUTd(0);                         // ClrImportant: Number of colors important

	if (hasAudio) {
		// Audio stream list
		AVIOUT4("LIST");
		AVIOUTd(4 + 8 + 56 + 8 + 16);// Length of list in bytes
		AVIOUT4("strl");
		// The audio stream header
		AVIOUT4("strh");
		AVIOUTd(56);                // # of bytes to follow
		AVIOUT4("auds");
		AVIOUTd(0);                 // Format (Optionally)
		AVIOUTd(0);                 // Flags
		AVIOUTd(0);                 // Reserved, MS says: wPriority, wLanguage
		AVIOUTd(0);                 // InitialFrames
		AVIOUTd(4);                 // Scale
		AVIOUTd(audiorate * 4);     // Rate, actual rate is scale/rate
		AVIOUTd(0);                 // Start
		AVIOUTd(audiowritten);      // Length
		AVIOUTd(0);                 // SuggestedBufferSize
		AVIOUTd(~0);                // Quality
		AVIOUTd(4);                 // SampleSize
		AVIOUTd(0);                 // Frame
		AVIOUTd(0);                 // Frame
		// The audio stream format
		AVIOUT4("strf");
		AVIOUTd(16);                // # of bytes to follow
		AVIOUTw(1);                 // Format, WAVE_ZMBV_FORMAT_PCM
		AVIOUTw(2);                 // Number of channels
		AVIOUTd(audiorate);         // SamplesPerSec
		AVIOUTd(audiorate * 4);     // AvgBytesPerSec
		AVIOUTw(4);                 // BlockAlign
		AVIOUTw(16);                // BitsPerSample
	}

	const char* versionStr = Version::FULL_VERSION.c_str();
	char dateStr[11];
	time_t t = time(NULL);
	struct tm *tm = localtime(&t);
	snprintf(dateStr, 11, "%04d-%02d-%02d", 1900 + tm->tm_year,
		 tm->tm_mon + 1, tm->tm_mday);
	AVIOUT4("LIST");
	AVIOUTd(4 // list type
		+ (4 + 4 + ((strlen(versionStr) + 1 + 1) & ~1)) // 1st chunk
		+ (4 + 4 + ((strlen(dateStr   ) + 1 + 1) & ~1)) // 2nd chunk
		); // size of the list
	AVIOUT4("INFO");
	AVIOUT4("ISFT");
	AVIOUTd(strlen(versionStr) + 1); // # of bytes to follow
	AVIOUTs(versionStr);
	AVIOUT4("ICRD");
	AVIOUTd(strlen(dateStr) + 1); // # of bytes to follow
	AVIOUTs(dateStr);
	// TODO: add artist (IART), comments (ICMT), name (INAM), etc.
	// use a loop over chunks (type + string) to create the above bytes in
	// a much nicer way

	// Finish stream list, i.e. put number of bytes in the list to proper pos
	int nmain = header_pos - main_list - 4;
	int njunk = AVI_HEADER_SIZE - 8 - 12 - header_pos;
	assert(njunk > 0); // increase AVI_HEADER_SIZE if this occurs
	AVIOUT4("JUNK");
	AVIOUTd(njunk);
	// Fix the size of the main list
	header_pos = main_list;
	AVIOUTd(nmain);
	header_pos = AVI_HEADER_SIZE - 12;

	AVIOUT4("LIST");
	AVIOUTd(written + 4);               // Length of list in bytes
	AVIOUT4("movi");

	// First add the index table to the end
	memcpy(&index[0], "idx1", 4);
	writeLE4(&index[4], index.size() - 8);
	fwrite(&index[0], 1, index.size(), file);

	fseek(file, 0, SEEK_SET);
	fwrite(&avi_header, 1, AVI_HEADER_SIZE, file);
	fclose(file);
}

void AviWriter::setFps(double fps_)
{
	fps = fps_;
}

void AviWriter::addAviChunk(const char* tag, unsigned size, void* data, unsigned flags)
{
	unsigned char chunk[8];
	chunk[0] = tag[0];
	chunk[1] = tag[1];
	chunk[2] = tag[2];
	chunk[3] = tag[3];
	writeLE4(&chunk[4], size);
	fwrite(chunk, 1, 8, file);

	unsigned writesize = (size + 1) & ~1;
	fwrite(data, 1, writesize, file);
	unsigned pos = written + 4;
	written += writesize + 8;

	unsigned idxsize = index.size();
	index.resize(idxsize + 16);
	index[idxsize + 0] = tag[0];
	index[idxsize + 1] = tag[1];
	index[idxsize + 2] = tag[2];
	index[idxsize + 3] = tag[3];
	writeLE4(&index[idxsize +  4], flags);
	writeLE4(&index[idxsize +  8], pos);
	writeLE4(&index[idxsize + 12], size);
}

static inline unsigned short bswap16(unsigned short val)
{
	return ((val & 0xFF00) >> 8) | ((val & 0x00FF) << 8);
}
void AviWriter::addFrame(const void** lineData, unsigned samples, short* sampleData)
{
	bool keyFrame = (frames++ % 300 == 0);
	void* buffer;
	unsigned size;
	codec->compressFrame(keyFrame, lineData, buffer, size);
	addAviChunk("00dc", size, buffer, keyFrame ? 0x10 : 0x0);

	if (samples) {
		assert(audiorate != 0);
		if (OPENMSX_BIGENDIAN) {
			short buf[2 * samples];
			for (unsigned i = 0; i < samples; ++i) {
				buf[2 * i + 0] = bswap16(sampleData[2 * i + 0]);
				buf[2 * i + 1] = bswap16(sampleData[2 * i + 1]);
			}
			addAviChunk("01wb", samples * 2 * sizeof(short), buf, 0);
		} else {
			addAviChunk("01wb", samples * 2 * sizeof(short), sampleData, 0);
		}
		audiowritten += samples;
	}
}

} // namespace openmsx
