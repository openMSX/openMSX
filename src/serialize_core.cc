#include "serialize_core.hh"
#include "serialize.hh"
#include "MSXException.hh"
#include "likely.hh"

namespace openmsx {

void enumError(const std::string_view str)
{
	throw MSXException("Invalid enum value: ", str);
}

void pointerError(unsigned id)
{
	throw MSXException("Couldn't find pointer in archive with id ", id);
}


static void versionError(const char* className, unsigned latestVersion, unsigned version)
{
	// note: the result of type_info::name() is implementation defined
	//       but should be ok to show in an error message
	throw MSXException(
		"your openMSX installation is too old (state contains type '",
		className, "' with version ", version,
		", while this openMSX installation only supports up to version ",
		latestVersion, ").");
}

unsigned loadVersionHelper(MemInputArchive& /*ar*/, const char* /*className*/,
                           unsigned /*latestVersion*/)
{
	UNREACHABLE; return 0;
}

unsigned loadVersionHelper(XmlInputArchive& ar, const char* className,
                           unsigned latestVersion)
{
	assert(ar.CAN_HAVE_OPTIONAL_ATTRIBUTES);
	auto version = ar.findAttributeAs<unsigned>("version");
	if (!version) return 1;
	if (unlikely(*version > latestVersion)) {
		versionError(className, latestVersion, *version);
	}
	return *version;
}

} // namespace openmsx
