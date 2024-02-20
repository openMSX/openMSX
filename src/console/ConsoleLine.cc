#include "ConsoleLine.hh"

#include "narrow.hh"
#include "ranges.hh"
#include "stl.hh"
#include "strCat.hh"
#include "utf8_unchecked.hh"
#include "view.hh"

namespace openmsx {

ConsoleLine::ConsoleLine(std::string line_, imColor color)
	: line(std::move(line_))
	, chunks(1, {color, 0})
{
}

void ConsoleLine::addChunk(std::string_view text, imColor color)
{
	chunks.emplace_back(Chunk{color, line.size()});
	line.append(text.data(), text.size());
}

void ConsoleLine::addLine(const ConsoleLine& ln)
{
	auto oldN = chunks.size();
	auto oldPos = line.size();

	strAppend(line, ln.line);

	append(chunks, ln.chunks);
	for (auto it = chunks.begin() + oldN, et = chunks.end(); it != et; ++it) {
		it->pos += oldPos;
	}
}

/** Split this line at a given column number.
	* This line will contain (up to) 'column' number of characters.
	* The remainder of the line is returned, this could be an empty line. */
ConsoleLine ConsoleLine::splitAtColumn(unsigned column)
{
	ConsoleLine result;
	if (line.empty()) return result;
	assert(!chunks.empty());

	auto it = line.begin(), et = line.end();
	while ((it != et) && column--) {
		utf8::unchecked::next(it);
	}
	auto pos = narrow<unsigned>(std::distance(line.begin(), it));

	auto it2 = ranges::upper_bound(chunks, pos, {}, &Chunk::pos);
	assert(it2 != chunks.begin());
	auto splitColor = it2[-1].color;

	if (it != et) {
		//result.addChunk(std::string_view{it, et}, splitColor); // c++20
		result.addChunk(std::string_view{&*it, size_t(et - it)}, splitColor);
		result.chunks.insert(result.chunks.end(), it2, chunks.end());
		for (auto& c : view::drop(result.chunks, 1)) {
			c.pos -= pos;
		}
	}

	line.erase(it, et);
	if (it2[-1].pos == pos) --it2;
	chunks.erase(it2, chunks.end());

	return result;
}

size_t ConsoleLine::numChars() const
{
	return utf8::unchecked::size(line);
}

imColor ConsoleLine::chunkColor(size_t i) const
{
	assert(i < chunks.size());
	return chunks[i].color;
}

std::string_view ConsoleLine::chunkText(size_t i) const
{
	assert(i < chunks.size());
	auto pos = chunks[i].pos;
	auto len = ((i + 1) == chunks.size())
	         ? std::string_view::npos
	         : chunks[i + 1].pos - pos;
	return std::string_view(line).substr(pos, len);
}

} // namespace openmsx
