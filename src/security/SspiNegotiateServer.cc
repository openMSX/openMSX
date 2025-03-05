#ifdef _WIN32

#include "SspiNegotiateServer.hh"
#include "MSXException.hh"

#include "one_of.hh"

#include <cstdint>

namespace openmsx {

using namespace sspiutils;

SspiNegotiateServer::SspiNegotiateServer(StreamWrapper& serverStream)
	: SspiPackageBase(serverStream, NEGOSSP_NAME_W)
{
	// We should probably cache the security descriptor, but this
	// isn't exactly a performance-sensitive part of the code
	psd = CreateCurrentUserSecurityDescriptor();
	if (!psd) {
		throw MSXException("CreateCurrentUserSecurityDescriptor failed");
	}
}

SspiNegotiateServer::~SspiNegotiateServer()
{
	LocalFree(psd);
}

bool SspiNegotiateServer::Authenticate()
{
	TimeStamp tsCredsExpiry;
	SECURITY_STATUS ss = AcquireCredentialsHandleW(
		nullptr,
		const_cast<SEC_WCHAR*>(NEGOSSP_NAME_W),
		SECPKG_CRED_INBOUND,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		&hCreds,
		&tsCredsExpiry);

	DebugPrintSecurityStatus("AcquireCredentialsHandleW", ss);
	if (ss != SEC_E_OK) {
		return false;
	}

	SecBufferDesc secClientBufferDesc, secServerBufferDesc;
	SecBuffer secClientBuffer, secServerBuffer;
	InitTokenContextBuffer(&secClientBufferDesc, &secClientBuffer);
	InitTokenContextBuffer(&secServerBufferDesc, &secServerBuffer);

	std::vector<char> buffer;
	PCtxtHandle phContext = nullptr;
	while (true) {
		// Receive another buffer from the client
		bool ret = RecvChunk(stream, buffer, cbMaxTokenSize);
		if (!ret) return false;

		secClientBuffer.cbBuffer = static_cast<unsigned long>(buffer.size());
		secClientBuffer.pvBuffer = &buffer[0];

		ULONG fContextAttr;
		TimeStamp tsContextExpiry;
		ss = AcceptSecurityContext(
			&hCreds,
			phContext,
			&secClientBufferDesc,
			ASC_REQ_ALLOCATE_MEMORY | ASC_REQ_CONNECTION,
			SECURITY_NETWORK_DREP,
			&hContext,
			&secServerBufferDesc,
			&fContextAttr,
			&tsContextExpiry);

		DebugPrintSecurityStatus("AcceptSecurityContext", ss);
		if (ss != one_of(SEC_E_OK, SEC_I_CONTINUE_NEEDED)) {
			return false;
		}

		// If we have something for the client, send it
		if (secServerBuffer.cbBuffer) {
			ret = SendChunk(stream, secServerBuffer.pvBuffer, secServerBuffer.cbBuffer);
			ClearContextBuffers(&secServerBufferDesc);
			if (!ret) return false;
		}

		// SEC_E_OK means that we're done
		if (ss == SEC_E_OK) {
			DebugPrintSecurityPackageName(&hContext);
			DebugPrintSecurityPrincipalName(&hContext);
			return true;
		}

		// Another time around the loop
		phContext = &hContext;
	}
}

bool SspiNegotiateServer::Authorize()
{
#ifdef __GNUC__
	// MinGW32's headers do not define QuerySecurityContextToken,
	// nor does its import library provide an export for it.
	// So when building with MinGW32, we load the export by hand.
	HMODULE secur32 = GetModuleHandleW(L"secur32.dll");
	if (!secur32) {
		return false;
	}
	auto QuerySecurityContextToken = reinterpret_cast<QUERY_SECURITY_CONTEXT_TOKEN_FN>(
		GetProcAddress(secur32, "QuerySecurityContextToken"));
	if (!QuerySecurityContextToken) {
		return false;
	}
#endif

	HANDLE hClientToken;
	SECURITY_STATUS ss = QuerySecurityContextToken(&hContext, &hClientToken);
	DebugPrintSecurityStatus("QuerySecurityContextToken", ss);
	if (ss != SEC_E_OK) {
		return false;
	}

	PRIVILEGE_SET privilegeSet;
	DWORD dwPrivSetSize = sizeof(privilegeSet);
	DWORD dwGranted;
	BOOL fAccess;
	BOOL ret = AccessCheck(
		psd,
		hClientToken,
		ACCESS_ALL,
		const_cast<PGENERIC_MAPPING>(&mapping),
		&privilegeSet,
		&dwPrivSetSize,
		&dwGranted,
		&fAccess);

	DebugPrintSecurityBool("AccessCheck", ret);
	DebugPrintSecurityPrincipalName(&hContext);

	CloseHandle(hClientToken);

	return ret && fAccess;
}

} // namespace openmsx

#endif
