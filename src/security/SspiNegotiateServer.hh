// $Id$

#ifndef SSPI_NEGOTIATE_SERVER_HH
#define SSPI_NEGOTIATE_SERVER_HH

#ifdef _WIN32

#include "SspiUtils.hh"

namespace openmsx {

using namespace sspiutils;

class SspiNegotiateServer : public SspiPackageBase
{
private:
	PSECURITY_DESCRIPTOR psd;
public:
	SspiNegotiateServer(StreamWrapper& serverStream);
	~SspiNegotiateServer();

	bool Authenticate();
	bool Authorize();
};

} // namespace openmsx

#endif // _WIN32

#endif // SSPI_NEGOTIATE_SERVER_HH
