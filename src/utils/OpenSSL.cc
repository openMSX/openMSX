#include "OpenSSL.hh"

#include <array>
#include <bit>
#include <cstdio>
#include <cstring>
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
constexpr auto libraryCandidates = std::to_array<const char*>({
	"libssl-4-x64.dll", "libssl-4.dll",
	"libssl-3-x64.dll", "libssl-3.dll",
	"libssl-1_1-x64.dll", "libssl-1_1.dll",
	"libssl.dll", "ssleay32.dll"
});
#else
using DllHandle = void*;
DllHandle openSslLib(const char* name)
{
	return dlopen(name, RTLD_NOW | RTLD_LOCAL);
}
constexpr auto libraryCandidates = std::to_array<const char*>({
	// Homebrew (Apple Silicon / Intel) and system-wide installs
	"/opt/homebrew/opt/openssl@3/lib/libssl.3.dylib",
	"/opt/homebrew/opt/openssl@1.1/lib/libssl.1.1.dylib",
	"/usr/local/opt/openssl@3/lib/libssl.3.dylib",
	"/usr/local/opt/openssl@1.1/lib/libssl.1.1.dylib",
	"/usr/local/lib/libssl.dylib",
	"/opt/homebrew/lib/libssl.dylib",
	// Generic names (ldconfig cache on Linux, default dyld paths on macOS)
	"libssl.so.3", "libssl.so.1.1", "libssl.so",
	"libssl.3.dylib", "libssl.dylib"
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
//  Resolved OpenSSL entry points (all optional except the core set)
// ---------------------------------------------------------------------

struct OpenSSLApi {
	// init
	int (*init)(unsigned long long opts, const SslInitSettings* settings) = nullptr;
	int (*libraryInit)() = nullptr;
	// methods / contexts
	const SslMethod* (*tlsClientMethod)() = nullptr;
	const SslMethod* (*ssl23ClientMethod)() = nullptr;
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
	X509* (*get1PeerCertificate)(const Ssl* ssl) = nullptr;
	void (*freeX509)(X509* x) = nullptr;
	unsigned long (*errGetError)() = nullptr;
	void (*errErrorStringN)(unsigned long e, char* buf, size_t len) = nullptr;
	const char* (*version)(int type) = nullptr;
};

// All mutable runtime state, held in a function-local singleton so there are
// no non-const namespace-scope variables (SonarCloud cpp:S1132).
struct Runtime {
	OpenSSLApi api;
	DllHandle libHandle = nullptr;
	DllHandle cryptoHandle = nullptr;
	bool loadAttempted = false;
	bool loaded = false;
};

Runtime& runtime()
{
	static Runtime r;
	return r;
}

constexpr unsigned long OPENSSL_INIT_LOAD_SSL_STRINGS = 0x00200000UL;
constexpr unsigned long OPENSSL_INIT_LOAD_CRYPTO_STRINGS = 0x00000001UL;

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

// ---------------------------------------------------------------------
//  Trust store setup
// ---------------------------------------------------------------------

#ifdef _WIN32
// Exports the Windows system root certificates as a PEM file and loads it
// into the given context, so that the device trusts the same CAs as the
// host operating system.
bool loadWindowsTrustStore(SslCtx* ctx)
{
	const OpenSSLApi& api = runtime().api;
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
		ok = api.ctxLoadVerifyLocations && api.ctxLoadVerifyLocations(ctx, tmpFile.data(), nullptr) == 1;
	}
	CertCloseStore(hStore, 0);
	DeleteFileA(tmpFile.data());
	return ok;
}
#endif

void setupTrustStore(SslCtx* ctx)
{
	const OpenSSLApi& api = runtime().api;
#ifdef _WIN32
	// The Windows system store provides the full root CA list
	loadWindowsTrustStore(ctx);
#else
	// macOS keeps the root certificates at a well-known location that is
	// not part of OpenSSL's default search path (libressl uses /etc/ssl
	// with a symlink, but Homebrew builds do not)
	if (api.ctxLoadVerifyLocations) {
		api.ctxLoadVerifyLocations(ctx, "/etc/ssl/cert.pem", nullptr);
	}
#endif
	if (api.ctxSetDefaultVerifyPaths) {
		api.ctxSetDefaultVerifyPaths(ctx);
	}
}

// Shared client context, created once on first use
SslCtx* getSharedContext()
{
	static std::once_flag flag;
	static SslCtx* ctx = nullptr;
	std::call_once(flag, [] {
		const auto& api = runtime().api;
		const SslMethod* method = api.tlsClientMethod ? api.tlsClientMethod() : nullptr;
		if (!method && api.ssl23ClientMethod) {
			method = api.ssl23ClientMethod();
		}
		if (!method || !api.ctxNew) return;
		ctx = api.ctxNew(method);
		if (ctx) {
			setupTrustStore(ctx);
		}
	});
	return ctx;
}

// ---------------------------------------------------------------------
//  Symbol resolution
// ---------------------------------------------------------------------

template<typename T>
T resolve(DllHandle lib, const char* name)
{
	return resolveSymbol<T>(lib, name);
}

// Tries the SSL library first, then the crypto library: the X509/ERR
// entry points live in libcrypto (GetProcAddress does not search DLL
// dependencies, so on Windows the crypto library must be loaded too)
template<typename T>
T resolveAny(const char* name)
{
	if (auto p = resolveSymbol<T>(runtime().libHandle, name)) {
		return p;
	}
	if (runtime().cryptoHandle) {
		if (auto p = resolveSymbol<T>(runtime().cryptoHandle, name)) {
			return p;
		}
	}
	return nullptr;
}

// Name of the libcrypto DLL that belongs to a libssl DLL
std::string cryptoNameFor(const char* sslName)
{
	std::string n = sslName;
	if (auto pos = n.find("libssl"); pos != std::string::npos) {
		n.replace(pos, 6, "libcrypto");
	} else if (n == "ssleay32.dll") {
		n = "libeay32.dll";
	}
	return n;
}

bool resolveSymbols()
{
	OpenSSLApi& api = runtime().api;
	DllHandle& libHandle = runtime().libHandle;
	api.init = resolve<decltype(api.init)>(libHandle, "OPENSSL_init_ssl");
	api.libraryInit = resolve<decltype(api.libraryInit)>(libHandle, "SSL_library_init");
	api.tlsClientMethod = resolve<decltype(api.tlsClientMethod)>(libHandle, "TLS_client_method");
	api.ssl23ClientMethod = resolve<decltype(api.ssl23ClientMethod)>(libHandle, "SSLv23_client_method");
	api.ctxNew = resolve<decltype(api.ctxNew)>(libHandle, "SSL_CTX_new");
	api.ctxSetDefaultVerifyPaths = resolve<decltype(api.ctxSetDefaultVerifyPaths)>(libHandle, "SSL_CTX_set_default_verify_paths");
	api.ctxLoadVerifyLocations = resolve<decltype(api.ctxLoadVerifyLocations)>(libHandle, "SSL_CTX_load_verify_locations");
	api.newSession = resolve<decltype(api.newSession)>(libHandle, "SSL_new");
	api.freeSession = resolve<decltype(api.freeSession)>(libHandle, "SSL_free");
	api.setFd = resolve<decltype(api.setFd)>(libHandle, "SSL_set_fd");
	api.setVerify = resolve<decltype(api.setVerify)>(libHandle, "SSL_set_verify");
	api.connect = resolve<decltype(api.connect)>(libHandle, "SSL_connect");
	api.ctrl = resolve<decltype(api.ctrl)>(libHandle, "SSL_ctrl");
	api.get0Param = resolve<decltype(api.get0Param)>(libHandle, "SSL_get0_param");
	api.verifyParamSet1Host = resolveAny<decltype(api.verifyParamSet1Host)>("X509_VERIFY_PARAM_set1_host");
	api.read = resolve<decltype(api.read)>(libHandle, "SSL_read");
	api.write = resolve<decltype(api.write)>(libHandle, "SSL_write");
	api.pending = resolve<decltype(api.pending)>(libHandle, "SSL_pending");
	api.shutdown = resolve<decltype(api.shutdown)>(libHandle, "SSL_shutdown");
	api.getError = resolve<decltype(api.getError)>(libHandle, "SSL_get_error");
	api.getVerifyResult = resolve<decltype(api.getVerifyResult)>(libHandle, "SSL_get_verify_result");
	api.get1PeerCertificate = resolve<decltype(api.get1PeerCertificate)>(libHandle, "SSL_get1_peer_certificate");
	api.freeX509 = resolveAny<decltype(api.freeX509)>("X509_free");
	api.errGetError = resolveAny<decltype(api.errGetError)>("ERR_get_error");
	api.errErrorStringN = resolveAny<decltype(api.errErrorStringN)>("ERR_error_string_n");
	api.version = resolveAny<decltype(api.version)>("OPENSSL_version");
	if (!api.version) {
		// LibreSSL exports the version function with this exact name
		api.version = resolveAny<decltype(api.version)>("OpenSSL_version");
	}
	if (!api.version) {
		api.version = resolveAny<decltype(api.version)>("SSLeay_version");
	}

	// Core set: without these there is nothing to work with
	if (!api.init && !api.libraryInit) return false;
	if (!api.tlsClientMethod && !api.ssl23ClientMethod) return false;
	if (!api.ctxNew || !api.newSession || !api.freeSession ||
	    !api.setFd || !api.setVerify || !api.connect || !api.read ||
	    !api.write || !api.shutdown || !api.getError) {
		return false;
	}

	// Initialize the library (idempotent in OpenSSL >= 1.1)
	if (api.init) {
		if (api.init(OPENSSL_INIT_LOAD_SSL_STRINGS |
		             OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr) != 1) {
			return false;
		}
	} else if (api.libraryInit() != 1) {
		return false;
	}
	return true;
}

bool openLibrary()
{
	for (const char* name : libraryCandidates) {
		if (auto h = openSslLib(name)) {
			runtime().libHandle = h;
			// Load the matching libcrypto as well: the X509/ERR entry
			// points are exported by libcrypto, and GetProcAddress does
			// not search DLL dependencies (best effort; the TLS session
			// functions themselves all live in libssl).
			if (auto ch = openSslLib(cryptoNameFor(name).c_str())) {
				runtime().cryptoHandle = ch;
			}
			return true;
		}
	}
	return false;
}

} // namespace

// ---------------------------------------------------------------------
//  OpenSSL public interface
// ---------------------------------------------------------------------

LibHandle* load()
{
	static LibHandle singleton; // only valid (and non-null) after a successful load
	if (runtime().loadAttempted) return runtime().loaded ? &singleton : nullptr;
	runtime().loadAttempted = true;
	if (!openLibrary()) return nullptr;
	runtime().loaded = resolveSymbols();
	return runtime().loaded ? &singleton : nullptr;
}

SessionHandle::SessionHandle(Ssl* ssl_)
	: ssl(ssl_)
{
}

SessionHandle::SessionHandle(SessionHandle&& other) noexcept
	: ssl(std::exchange(other.ssl, nullptr))
{
}

SessionHandle& SessionHandle::operator=(SessionHandle&& other) noexcept
{
	if (this != &other) {
		release();
		ssl = std::exchange(other.ssl, nullptr);
	}
	return *this;
}

SessionHandle::~SessionHandle()
{
	release();
}

void SessionHandle::release() noexcept
{
	const OpenSSLApi& api = runtime().api;
	if (!ssl || !api.shutdown || !api.freeSession) return;
	// Best-effort shutdown: the peer may already be gone
	api.shutdown(ssl);
	api.freeSession(ssl);
	ssl = nullptr;
}

int SessionHandle::handshake() const
{
	const OpenSSLApi& api = runtime().api;
	if (!api.connect || !api.getError) return -1;
	int r = api.connect(ssl);
	if (r == 1) return 1;
	int e = api.getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ) return 0;
	if (e == SSL_ERROR_WANT_WRITE) return 2;
	return -1;
}

int SessionHandle::verifyResult() const
{
	const OpenSSLApi& api = runtime().api;
	if (!api.getVerifyResult) return 0;
	long vr = api.getVerifyResult(ssl);
	if (vr == X509_V_OK) return 0;

	// The server did not provide a certificate at all
	if (api.get1PeerCertificate && api.freeX509) {
		X509* peer = api.get1PeerCertificate(ssl);
		if (peer) {
			api.freeX509(peer);
		} else {
			return 9; // TLS: server did not provide a certificate
		}
	}

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

IoResult SessionHandle::read(std::span<char> buf) const
{
	const OpenSSLApi& api = runtime().api;
	using enum IoError;
	if (!api.read || !api.getError) return std::unexpected(Failed);
	int r = api.read(ssl, buf.data(), static_cast<int>(buf.size()));
	if (r > 0) return IoResult(r);
	int e = api.getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
		return std::unexpected(WouldBlock);
	}
	if (e == SSL_ERROR_ZERO_RETURN) {
		return std::unexpected(Closed); // clean TLS close
	}
	return std::unexpected(Failed);
}

IoResult SessionHandle::write(std::span<const char> buf) const
{
	const OpenSSLApi& api = runtime().api;
	using enum IoError;
	if (!api.write || !api.getError) return std::unexpected(Failed);
	int r = api.write(ssl, buf.data(), static_cast<int>(buf.size()));
	if (r > 0) return IoResult(r);
	int e = api.getError(ssl, r);
	if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) {
		return std::unexpected(WouldBlock);
	}
	if (e == SSL_ERROR_ZERO_RETURN) {
		return std::unexpected(Closed); // peer sent close_notify
	}
	return std::unexpected(Failed);
}

int SessionHandle::pending() const
{
	const OpenSSLApi& api = runtime().api;
	return api.pending ? api.pending(ssl) : 0;
}

std::optional<SessionHandle> LibHandle::createClientSession(
	bool verify, zstring_view hostname, int fd) const
{
	const OpenSSLApi& api = runtime().api;
	if (!runtime().loaded) return std::nullopt;
	SslCtx* ctx = getSharedContext();
	if (!ctx) return std::nullopt;
	if (!api.newSession || !api.setFd || !api.freeSession || !api.setVerify) {
		return std::nullopt;
	}

	Ssl* ssl = api.newSession(ctx);
	if (!ssl) return std::nullopt;
	if (api.setFd(ssl, fd) != 1) {
		api.freeSession(ssl);
		return std::nullopt;
	}
	api.setVerify(ssl, verify ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, nullptr);

	// Server name: sent as SNI and, when verifying, used to check the
	// host name in the server certificate
	if (!hostname.empty()) {
		if (api.ctrl) {
			api.ctrl(ssl, SSL_CTRL_SET_TLSEXT_HOSTNAME,
			         TLSEXT_NAMETYPE_HOST_NAME,
			         hostname.data());
		}
		if (verify && api.get0Param && api.verifyParamSet1Host) {
			api.verifyParamSet1Host(api.get0Param(ssl), hostname.data(),
			                        hostname.size());
		}
	}
	return SessionHandle(ssl);
}

zstring_view LibHandle::version() const
{
	const OpenSSLApi& api = runtime().api;
	return api.version ? api.version(OPENSSL_VERSION) : "";
}

zstring_view LibHandle::last_error() const
{
	if (const OpenSSLApi& api = runtime().api;
	    api.errGetError && api.errErrorStringN) {
		thread_local std::array<char, 256> buf;
		api.errErrorStringN(api.errGetError(), buf.data(), buf.size());
		return buf.data();
	}
	return "OpenSSL error";
}

} // namespace openmsx::OpenSSL
