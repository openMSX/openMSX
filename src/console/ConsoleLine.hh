#ifndef CONSOLE_LINE_HH
#define CONSOLE_LINE_HH

#include "ImGuiUtils.hh"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace openmsx {

/** This class represents a single text line in the console.
  * The line can have several chunks with different colors. */
class ConsoleLine
{
public:
	/** Construct empty line. */
	ConsoleLine() = default;

	/** Reinitialize to an empty line. */
	void clear() {
		line.clear();
		chunks.clear();
	}

	/** Construct line with a single color (by default white). */
	explicit ConsoleLine(std::string line, imColor color = imColor::TEXT);

	/** Append a chunk with a (different) color. */
	void addChunk(std::string_view text, imColor color = imColor::TEXT);

	/** Append another line (possibly containing multiple chunks). */
	void addLine(const ConsoleLine& ln);

	/** Split this line at a given column number.
	  * This line will contain (up to) 'column' number of characters.
	  * The remainder of the line is returned, this could be an empty line. */
	ConsoleLine splitAtColumn(unsigned column);

	/** Get the number of UTF8 characters in this line. So multi-byte
	  * characters are counted as a single character. */
	[[nodiscard]] size_t numChars() const;
	/** Get the total string, ignoring color differences. */
	[[nodiscard]] const std::string& str() const { return line; }

	/** Get the number of different chunks. Each chunk is a a part of the
	  * line that has the same color. */
	[[nodiscard]] size_t numChunks() const { return chunks.size(); }
	/** Get the color for the i-th chunk. */
	[[nodiscard]] imColor chunkColor(size_t i) const;
	/** Get the text for the i-th chunk. */
	[[nodiscard]] std::string_view chunkText(size_t i) const;

	[[nodiscard]] const auto& getChunks() const { return chunks; }

public:
	struct Chunk {
		imColor color;
		std::string_view::size_type pos;
	};

private:
	std::string line;
	std::vector<Chunk> chunks;
};

} // namespace openmsx

#endif
