// $Id$

#include "EnumSetting.hh"
#include <sstream>

using std::ostringstream;


namespace openmsx {

IntStringMap::IntStringMap(BaseMap *map_)
{
	this->stringToInt = map_;
}

IntStringMap::~IntStringMap()
{
	delete stringToInt;
}

const string &IntStringMap::lookupInt(int n) const
{
	for (MapIterator it = stringToInt->begin()
	; it != stringToInt->end() ; ++it) {
		if (it->second == n) return it->first;
	}
	throw MSXException("Integer not in map: " + n);
}

int IntStringMap::lookupString(const string &s) const
{
	MapIterator it = stringToInt->find(s);
	if (it != stringToInt->end()) return it->second;
	// TODO: Don't use the knowledge that we're called inside command
	//       processing: use a different exception.
	throw CommandException("set: " + s + ": not a valid value");
}

string IntStringMap::getSummary() const
{
	ostringstream out;
	MapIterator it = stringToInt->begin();
	out << it->first;
	for (++it; it != stringToInt->end(); ++it) {
		out << ", " << it->first;
	}
	return out.str();
}

void IntStringMap::getStringSet(set<string>& result) const
{
	for (MapIterator it = stringToInt->begin()
	; it != stringToInt->end(); ++it) {
		result.insert(it->first);
	}
}

} // namespace openmsx
