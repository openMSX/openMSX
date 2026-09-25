#include "OpenSSL.hh"

#include <array>
#include <bit>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <dlfcn.h>
#endif

namespace openmsx::OpenSSL {

// Opaque OpenSSL runtime types, used only by the resolved entry points
// (forward-declared here so that openMSX never needs the OpenSSL
// development headers).
struct SslCtx;
struct X509;
struct X509VerifyParam;
struct SslMethod;
struct X509StoreCtx;
struct SslInitSettings;

// Resolved OpenSSL entry points. Every member is mandatory (load() refuses
// the library when any is missing); the only exception is `version`, whose
// three name spellings differ across OpenSSL/LibreSSL and which is
// diagnostic-only.
struct OpenSSLApi {
	// init
	int (*init)(unsigned long long opts, const SslInitSettings* settings) = nullptr;
	// methods / contexts
	const SslMethod* (*tlsClientMethod)() = nullptr;
	SslCtx* (*ctxNew)(const SslMethod* method) = nullptr;
	int (*ctxSetDefaultVerifyPaths)(SslCtx* ctx) = nullptr;
	int (*ctxLoadVerifyLocations)(SslCtx* ctx, const char* caFile,
	                              const char* caPath) = nullptr;
	// sessions
	Ssl* (*newSession)(SslCtx* ctx) = nullptr;
	void (*freeSession)(Ssl* ssl) = nullptr;
	int (*setFd)(Ssl* ssl, int fd) = nullptr;
	void (*setVerify)(Ssl* ssl, int mode, void (*)(int, X509StoreCtx*)) = nullptr;
	int (*connect)(Ssl* ssl) = nullptr;
	long (*ctrl)(Ssl* ssl, int cmd, long larg, const char* parg) = nullptr;
	X509VerifyParam* (*get0Param)(const Ssl* ssl) = nullptr;
	int (*verifyParamSet1Host)(X509VerifyParam* param, const char* name,
	                           size_t namelen) = nullptr;
	// data
	int (*read)(Ssl* ssl, char* buf, int num) = nullptr;
	int (*write)(Ssl* ssl, const char* buf, int num) = nullptr;
	int (*pending)(const Ssl* ssl) = nullptr;
	int (*shutdown)(Ssl* ssl) = nullptr;
	// errors / verification
	int (*getError)(const Ssl* ssl, int ret) = nullptr;
	long (*getVerifyResult)(const Ssl* ssl) = nullptr;
	const char* (*verifyCertErrorString)(long code) = nullptr;
	X509* (*get1PeerCertificate)(const Ssl* ssl) = nullptr;
	void (*freeX509)(X509* x) = nullptr;
	unsigned long (*errGetError)() = nullptr;
	void (*errErrorStringN)(unsigned long e, char* buf, size_t len) = nullptr;
	const char* (*version)(int type) = nullptr;
};

namespace {

// ---------------------------------------------------------------------
//  Runtime library handle
// ---------------------------------------------------------------------

#ifdef _WIN32
using DllHandle = HMODULE;
DllHandle openSslLib(const char* name)
{
	return LoadLibraryA(name);
}
#else
using DllHandle = void*;
DllHandle openSslLib(const char* name)
{
	return dlopen(name, RTLD_NOW | RTLD_LOCAL);
}
#endif

#ifdef _WIN32
constexpr auto libraryCandidates = std::to_array<const char*>({
	"libssl-4-x64.dll", "libssl-4.dll",
	"libssl-3-x64.dll", "libssl-3.dll",
	"libssl-1_1-x64.dll", "libssl-1_1.dll",
	"libssl.dll"
});
#elif defined(__APPLE__)
constexpr auto libraryCandidates = std::to_array<const char*>({
	// Homebrew (Apple Silicon / Intel) and system-wide installs
	"/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib",
	"/opt/homebrew/opt/openssl@1.1/lib/libssl.1.1.dylib",
	"/usr/local/opt/openssl@3/lib/libssl.3.dylib",
	"/usr/local/opt/openssl@1.1/lib/libssl.1.1.dylib",
	"/usr/local/lib/libssl.dylib",
	"/opt/homebrew/lib/libssl.dylib",
	// Generic names (default dyld search paths)
	"libssl.3.dylib", "libssl.1.1.dylib", "libssl.dylib"
});
#else
constexpr auto libraryCandidates = std::to_array<const char*>({
	// Generic names (ldconfig cache)
	"libssl.so.3", "libssl.so.1.1", "libssl.so"
});
#endif

// Resolves one exported symbol, reinterpreted to the type the caller
// expects. Returns nullptr when it is not present. All pointer types have
// the same width, so this is ABI-safe.
template<typename T>
T resolveSymbol(DllHandle lib, const char* name)
{
#ifdef _WIN32
	return std::bit_cast<T>(GetProcAddress(lib, name));
#else
	return std::bit_cast<T>(dlsym(lib, name));
#endif
}

// ---------------------------------------------------------------------
//  Trust store setup
// ---------------------------------------------------------------------

#ifdef _WIN32
// Exports the Windows system root certificates as a PEM file and loads it
// into the given context, so that the device trusts the same CAs as the
// host operating system.
bool loadWindowsTrustStore(const OpenSSLApi& api, SslCtx* ctx)
{
	// The Windows system store provides the full root CA list
	HCERTSTORE hStore = CertOpenStore(CERT_STORE_PROV_SYSTEM_A, 0, 0,
	                                  CERT_SYSTEM_STORE_CURRENT_USER,
	                                  "ROOT");
	if (!hStore) return false;

	std::array<char, MAX_PATH> tmpPath;
	std::array<char, MAX_PATH> tmpFile;
	if (!GetTempPathA(MAX_PATH, tmpPath.data())) {
		CertCloseStore(hStore, 0);
		return false;
	}
	// GetTempFileNameA creates a unique empty file in the temp directory
	if (!GetTempFileNameA(tmpPath.data(), "omsx", 0, tmpFile.data())) {
		CertCloseStore(hStore, 0);
		return false;
	}

	bool ok = false;
	if (FILE* f = fopen(tmpFile.data(), "wb"); f) {
		PCCERT_CONTEXT pCert = nullptr;
		while ((pCert = CertEnumCertificatesInStore(hStore, pCert)) != nullptr) {
			DWORD len = 0;
			if (!CryptBinaryToStringA(pCert->pbCertEncoded,
			                          pCert->cbCertEncoded,
			                          CRYPT_STRING_BASE64HEADER,
			                          nullptr, &len) || len == 0) {
				continue;
			}
			std::vector<char> buf(len);
			if (CryptBinaryToStringA(pCert->pbCertEncoded,
			                         pCert->cbCertEncoded,
			                         CRYPT_STRING_BASE64HEADER,
			                         buf.data(), &len)) {
				fwrite(buf.data(), 1, len, f);
			}
		}
		fclose(f);
		ok = api.ctxLoadVerifyLocations(ctx, tmpFile.data(), nullptr) == 1;
	}
	CertCloseStore(hStore, 0);
	DeleteFileA(tmpFile.data());
	return ok;
}
#endif

// Best-effort trust-store setup: failures only reduce what can be
// verified, they never prevent TLS, so the return values are ignored.
void setupTrustStore(const OpenSSLApi& api, SslCtx* ctx)
{
#ifdef _WIN32
	// The Windows system store provides the full root CA list
	loadWindowsTrustStore(api, ctx);
#else
	// Load the common POSIX CA bundle first: it is needed on macOS
	// (Homebrew builds do not use OpenSSL's compiled-in default paths)
	// and covers Linux distros whose bundle is not on those paths either.
	// Best effort: when the file is missing, the default paths below may
	// still work.
	api.ctxLoadVerifyLocations(ctx, "/etc/ssl/cert.pem", nullptr);
#endif
	// The library's compiled-in default paths (e.g. /etc/ssl/certs on
	// Linux); on macOS Homebrew builds this fails, but the bundle above
	// was already loaded.
	api.ctxSetDefaultVerifyPaths(ctx);
}

// Shared client context, created once on first use
SslCtx* getSharedContext(const OpenSSLApi& api)
{
	static std::once_flag flag;
	static SslCtx* ctx = nullptr;
	std::call_once(flag, [&api] {
		// TLS_client_method and SSL_CTX_new are mandatory entry points
		ctx = api.ctxNew(api.tlsClientMethod());
		if (ctx) {
			setupTrustStore(api, ctx);
		}
	});
	return ctx;
}

// ---------------------------------------------------------------------
//  Symbol resolution
// ---------------------------------------------------------------------

constexpr unsigned long OPENSSL_INIT_LOAD_SSL_STRINGS = 0x00200000UL;
constexpr unsigned long OPENSSL_INIT_LOAD_CRYPTO_STRINGS = 0x00000001UL;

template<typename T>
T resolve(DllHandle lib, const char* name)
{
	return resolveSymbol<T>(lib, name);
}

// Tries the SSL library first, then the crypto library: the X509/ERR
// entry points live in libcrypto (GetProcAddress does not search DLL
// dependencies, so on Windows the crypto library must be loaded too)
template<typename T>
T resolveAny(DllHandle lib, DllHandle crypto, const char* name)
{
	if (auto p = resolveSymbol<T>(lib, name)) {
		return p;
	}
	if (crypto) {
		if (auto p = resolveSymbol<T>(crypto, name)) {
			return p;
		}
	}
	return nullptr;
}

// Name of the libcrypto DLL that belongs to a libssl DLL
std::string cryptoNameFor(std::string n)
{
	if (auto pos = n.find("libssl"); pos != std::string::npos) {
		n.replace(pos, 6, "libcrypto");
	}
	return n;
}

bool openLibrary(DllHandle& sslLib, DllHandle& cryptoLib)
{
	for (const char* name : libraryCandidates) {
		if (auto h = openSslLib(name)) {
			sslLib = h;
			// Load the matching libcrypto as well: the X509/ERR entry
			// points are exported by libcrypto, and GetProcAddress does
			// not search DLL dependencies (best effort; the TLS session
			// functions themselves all live in libssl).
			std::string cryptoName = cryptoNameFor(name);
			if (auto ch = openSslLib(cryptoName.c_str())) {
				cryptoLib = ch;
			}
			return true;
		}
	}
	return false;
}

bool resolveSymbols(OpenSSLApi& api, DllHandle sslLib, DllHandle cryptoLib)
{
	api.init = resolve<decltype(api.init)>(sslLib, "OPENSSL_init_ssl");
	api.tlsClientMethod = resolve<decltype(api.tlsClientMethod)>(sslLib, "TLS_client_method");
	api.ctxNew = resolve<decltype(api.ctxNew)>(sslLib, "SSL_CTX_new");
	api.ctxSetDefaultVerifyPaths = resolve<decltype(api.ctxSetDefaultVerifyPaths)>(sslLib, "SSL_CTX_set_default_verify_paths");
	api.ctxLoadVerifyLocations = resolve<decltype(api.ctxLoadVerifyLocations)>(sslLib, "SSL_CTX_load_verify_locations");
	api.newSession = resolve<decltype(api.newSession)>(sslLib, "SSL_new");
	api.freeSession = resolve<decltype(api.freeSession)>(sslLib, "SSL_free");
	api.setFd = resolve<decltype(api.setFd)>(sslLib, "SSL_set_fd");
	api.setVerify = resolve<decltype(api.setVerify)>(sslLib, "SSL_set_verify");
	api.connect = resolve<decltype(api.connect)>(sslLib, "SSL_connect");
	api.ctrl = resolve<decltype(api.ctrl)>(sslLib, "SSL_ctrl");
	api.get0Param = resolve<decltype(api.get0Param)>(sslLib, "SSL_get0_param");
	api.verifyParamSet1Host = resolveAny<decltype(api.verifyParamSet1Host)>(sslLib, cryptoLib, "X509_VERIFY_PARAM_set1_host");
	api.read = resolve<decltype(api.read)>(sslLib, "SSL_read");
	api.write = resolve<decltype(api.write)>(sslLib, "SSL_write");
	api.pending = resolve<decltype(api.pending)>(sslLib, "SSL_pending");
	api.shutdown = resolve<decltype(api.shutdown)>(sslLib, "SSL_shutdown");
	api.getError = resolve<decltype(api.getError)>(sslLib, "SSL_get_error");
	api.getVerifyResult = resolve<decltype(api.getVerifyResult)>(sslLib, "SSL_get_verify_result");
	api.verifyCertErrorString = resolveAny<decltype(api.verifyCertErrorString)>(sslLib, cryptoLib, "X509_verify_cert_error_string");
	api.get1PeerCertificate = resolve<decltype(api.get1PeerCertificate)>(sslLib, "SSL_get1_peer_certificate");
	api.freeX509 = resolveAny<decltype(api.freeX509)>(sslLib, cryptoLib, "X509_free");
	api.errGetError = resolveAny<decltype(api.errGetError)>(sslLib, cryptoLib, "ERR_get_error");
	api.errErrorStringN = resolveAny<decltype(api.errErrorStringN)>(sslLib, cryptoLib, "ERR_error_string_n");
	api.version = resolveAny<decltype(api.version)>(sslLib, cryptoLib, "OPENSSL_version");
	if (!api.version) {
		// LibreSSL exports the version function with this exact name
		api.version = resolveAny<decltype(api.version)>(sslLib, cryptoLib, "OpenSSL_version");
	}
	if (!api.version) {
		api.version = resolveAny<decltype(api.version)>(sslLib, cryptoLib, "SSLeay_version");
	}

	// All entry points are mandatory (except the version spellings above,
	// which are diagnostic-only): refuse to load the library when
	// anything essential is missing. This requires OpenSSL >= 1.1 or
	// LibreSSL >= 3.5.
	if (!api.init || !api.tlsClientMethod || !api.ctxNew ||
	    !api.ctxSetDefaultVerifyPaths || !api.ctxLoadVerifyLocations ||
	    !api.newSession || !api.freeSession || !api.setFd || !api.setVerify ||
	    !api.connect || !api.ctrl || !api.get0Param || !api.verifyParamSet1Host ||
	    !api.read || !api.write || !api.pending || !api.shutdown || !api.getError ||
	    !api.getVerifyResult || !api.verifyCertErrorString ||
	    !api.get1PeerCertificate || !api.freeX509 ||
	    !api.errGetError || !api.errErrorStringN) {
		return false;
	}

	// Initialize the library (idempotent in OpenSSL >= 1.1)
	if (api.init(OPENSSL_INIT_LOAD_SSL_STRINGS |
	             OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr) != 1) {
		return false;
	}
	return true;
}

constexpr int SSL_VERIFY_NONE = 0x00;
constexpr int SSL_VERIFY_PEER = 0x01;
constexpr int SSL_ERROR_WANT_READ = 2;
constexpr int SSL_ERROR_WANT_WRITE = 3;
constexpr int SSL_ERROR_ZERO_RETURN = 6;
constexpr int SSL_CTRL_SET_TLSEXT_HOSTNAME = 55;
constexpr long TLSEXT_NAMETYPE_HOST_NAME = 0;
constexpr int OPENSSL_VERSION = 0;

// X509 verify error codes (used to map to TCP-IP UNAPI close reasons)
constexpr long X509_V_OK = 0;
constexpr long X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT = 2;
constexpr long X509_V_ERR_CERT_NOT_YET_VALID = 9;
constexpr long X509_V_ERR_CERT_HAS_EXPIRED = 10;
constexpr long X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT = 18;
constexpr long X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN = 19;
constexpr long X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY = 20;
constexpr long X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE = 21;
constexpr long X509_V_ERR_CERT_REVOKED = 23;
constexpr long X509_V_ERR_INVALID_CA = 24;
constexpr long X509_V_ERR_CERT_UNTRUSTED = 27;
constexpr long X509_V_ERR_CERT_REJECTED = 28;
constexpr long X509_V_ERR_HOSTNAME_MISMATCH = 62;

} // namespace

// ---------------------------------------------------------------------
//  OpenSSL public interface
// ---------------------------------------------------------------------

LibHandle::LibHandle()
	: api(std::make_unique<OpenSSLApi>())
{
}

LibHandle::~LibHandle() = default;

LibHandle* load()
{
	static LibHandle singleton;
	static std::once_flag flag;
	std::call_once(flag, [] {
		// The handles are only needed during symbol resolution; the
		// loaded libraries stay resident for the process lifetime.
		DllHandle sslLib = nullptr;
		DllHandle cryptoLib = nullptr;
		if (openLibrary(sslLib, cryptoLib)) {
			singleton.loaded = resolveSymbols(*singleton.api, sslLib, cryptoLib);
			singleton.libHandle = sslLib;
			singleton.cryptoHandle = cryptoLib;
		}
	});
	return singleton.loaded ? &singleton : nullptr;
}

SessionHandle::SessionHandle(Ssl* ssl_, const OpenSSLApi* api_)
	: ssl(ssl_)
	, api(api_)
{
}

SessionHandle::SessionHandle(SessionHandle&& other) noexcept
	: ssl(std::exchange(other.ssl, nullptr))
	, api(other.api)
{
}

SessionHandle& SessionHandle::operator=(SessionHandle&& other) noexcept
{
	if (this != &other) {
		release();
		ssl = std::exchange(other.ssl, nullptr);
		api = other.api;
	}
	return *this;
}

SessionHandle::~SessionHandle()
{
	release();
}

void SessionHandle::release() noexcept
{
	if (!ssl) return;
	// Best-effort shutdown: the peer may already be gone
	api->shutdown(ssl);
	api->freeSession(ssl);
	ssl = nullptr;
}

HandshakeResult SessionHandle::handshake() const
{
	using enum HandshakeResult::Type;
	int r = api->connect(ssl);
	if (r == 1) return HandshakeResult{Done};
	int e = api->getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ) return HandshakeResult{NeedRead};
	if (e == SSL_ERROR_WANT_WRITE) return HandshakeResult{NeedWrite};
	// Real error: capture the OpenSSL error code now, before any other
	// OpenSSL call can overwrite the error queue (mirrors serial::ErrorCode).
	return HandshakeResult{Failed, ErrorCode{api->errGetError()}};
}

int SessionHandle::verifyResult() const
{
	long vr = api->getVerifyResult(ssl);
	if (vr == X509_V_OK) return 0;

	// The server did not provide a certificate at all
	X509* peer = api->get1PeerCertificate(ssl);
	if (!peer) {
		return 9; // TLS: server did not provide a certificate
	}
	api->freeX509(peer);

	// Map the X509 verify error to a TCP-IP UNAPI close reason (spec 4.5.4)
	switch (vr) {
	case X509_V_ERR_HOSTNAME_MISMATCH:
		return 11; // host name didn't match
	case X509_V_ERR_CERT_HAS_EXPIRED:
	case X509_V_ERR_CERT_NOT_YET_VALID:
		return 12; // expired
	case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
	case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
		return 13; // self-signed
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT:
	case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
	case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
	case X509_V_ERR_CERT_UNTRUSTED:
	case X509_V_ERR_CERT_REJECTED:
		return 14; // untrusted root
	case X509_V_ERR_CERT_REVOKED:
		return 15; // revoked
	case X509_V_ERR_INVALID_CA:
		return 16; // invalid certificate authority
	default:
		return 10; // invalid server certificate
	}
}

std::string SessionHandle::verifyErrorDescription() const
{
	long vr = api->getVerifyResult(ssl);
	if (vr == X509_V_OK) return "";
	return api->verifyCertErrorString(vr);
}

IoResult SessionHandle::read(std::span<char> buf) const
{
	using enum IoError::Type;
	int r = api->read(ssl, buf.data(), static_cast<int>(buf.size()));
	if (r > 0) return IoResult(r);
	int e = api->getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
		return std::unexpected(IoError{WouldBlock});
	}
	if (e == SSL_ERROR_ZERO_RETURN) {
		return std::unexpected(IoError{Closed}); // clean TLS close
	}
	// Real error: capture the OpenSSL error code now, before any other
	// OpenSSL call can overwrite the error queue (mirrors serial::ErrorCode).
	return std::unexpected(IoError{Failed, ErrorCode{api->errGetError()}});
}

IoResult SessionHandle::write(std::span<const char> buf) const
{
	using enum IoError::Type;
	int r = api->write(ssl, buf.data(), static_cast<int>(buf.size()));
	if (r > 0) return IoResult(r);
	int e = api->getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
		return std::unexpected(IoError{WouldBlock});
	}
	if (e == SSL_ERROR_ZERO_RETURN) {
		return std::unexpected(IoError{Closed}); // peer sent close_notify
	}
	// Real error: capture the OpenSSL error code now, before any other
	// OpenSSL call can overwrite the error queue (mirrors serial::ErrorCode).
	return std::unexpected(IoError{Failed, ErrorCode{api->errGetError()}});
}

size_t SessionHandle::pending() const
{
	return static_cast<size_t>(api->pending(ssl));
}

std::optional<SessionHandle> LibHandle::createClientSession(
	bool verify, zstring_view hostname, int fd) const
{
	const OpenSSLApi& sslApi = *this->api;
	SslCtx* ctx = getSharedContext(sslApi);
	if (!ctx) return std::nullopt;
	Ssl* ssl = sslApi.newSession(ctx);
	if (!ssl) return std::nullopt;
	if (sslApi.setFd(ssl, fd) != 1) {
		sslApi.freeSession(ssl);
		return std::nullopt;
	}
	sslApi.setVerify(ssl, verify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);

	// Server name: sent as SNI and, when verifying, used to check the
	// host name in the server certificate
	if (!hostname.empty()) {
		sslApi.ctrl(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME,
		            TLSEXT_NAMETYPE_HOST_NAME,
		            hostname.data());
		if (verify) {
			sslApi.verifyParamSet1Host(sslApi.get0Param(ssl), hostname.data(),
			                           hostname.size());
		}
	}
	return SessionHandle(ssl, &sslApi);
}

zstring_view LibHandle::version() const
{
	const OpenSSLApi& sslApi = *this->api;
	return sslApi.version ? sslApi.version(OPENSSL_VERSION) : "";
}

std::string to_string(ErrorCode ec)
{
	// An ErrorCode can only have been produced by an I/O call on a
	// session, so the library is loaded here (load() is idempotent and
	// thread-safe). A value of 0 means no error was queued: report a
	// generic message.
	if (const auto* lib = load(); lib && ec.value != 0) {
		std::array<char, 256> buf;
		lib->api->errErrorStringN(ec.value, buf.data(), buf.size());
		return buf.data();
	}
	return "OpenSSL error";
}

} // namespace openmsx::OpenSSL