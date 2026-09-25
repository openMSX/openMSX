#ifndef OPENSSL_HH
#define OPENSSL_HH

#include "zstring_view.hh"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace openmsx::OpenSSL {

// Minimal TLS wrapper around the OpenSSL runtime library installed on the
// host, loaded dynamically at runtime (LoadLibrary / dlopen). Building
// openMSX itself does not require any OpenSSL development package: all
// OpenSSL types are opaque and every function is reached through resolved
// entry points.
//
// When OpenSSL is not installed, load() returns nullptr and the SMXWiFi
// device degrades gracefully: the TLS capability bits are not advertised
// and TCP_OPEN with the TLS flag returns ERR_NOT_IMP.
//
// Supported: OpenSSL 1.1.x and 3.x on Windows, Linux and macOS (LibreSSL
// 3.5 and newer is mostly API-compatible and will also be picked up as a
// fallback).

// Opaque handle for an OpenSSL runtime type, kept forward-declared so that
// openMSX itself never needs the OpenSSL development headers. The other
// opaque types used internally by the resolved entry points are declared
// in OpenSSL.cc.
struct Ssl;

// Resolved OpenSSL entry points (defined in OpenSSL.cc; only declared here
// so that the load state can live in the LibHandle singleton).
struct OpenSSLApi;

// OpenSSL error code captured at the moment of failure (the value returned
// by ERR_get_error(); 0 when the error queue was empty, e.g. a plain
// syscall failure reported through SSL_ERROR_SYSCALL). It is safe to store
// the value and format it later with to_string(): no other OpenSSL call
// can overwrite it in between (same pattern as serial::ErrorCode).
struct ErrorCode {
	unsigned long value = 0;
};

// Formats an OpenSSL error code, e.g.
// "error:1408A0C1:SSL routines:ssl3_get_record:wrong version number".
[[nodiscard]] std::string to_string(ErrorCode ec);

// Result of a (non-blocking) read() or write(): success carries the
// number of bytes transferred; on failure IoError says why.
struct IoError {
	enum class Type {
		WouldBlock, // the call would block; retry later (either readiness)
		Closed,     // clean TLS close (peer sent close_notify)
		Failed      // real error
	};
	Type type = Type::Failed;
	ErrorCode code; // meaningful when type == Type::Failed
};
using IoResult = std::expected<size_t, IoError>;

// Result of driving the TLS handshake (non-blocking). The caller must
// retry the handshake when the socket reaches the requested readiness.
// The values map 1:1 to the underlying SSL_connect()/SSL_get_error()
// contract.
struct HandshakeResult {
	enum class Type {
		NeedRead = 0,  // retry when the socket is readable
		Done = 1,      // handshake completed successfully
		NeedWrite = 2, // retry when the socket is writable
		Failed = -1    // handshake failed
	};
	Type type = Type::Failed;
	ErrorCode code; // meaningful when type == Type::Failed
};

// An active TLS session on a connected (non-blocking) socket. Move-only
// RAII: the destructor performs the best-effort shutdown and releases the
// session, so a SessionHandle can never outlive its session.
struct SessionHandle {
	SessionHandle(const SessionHandle&) = delete;
	SessionHandle& operator=(const SessionHandle&) = delete;
	SessionHandle(SessionHandle&& other) noexcept;
	SessionHandle& operator=(SessionHandle&& other) noexcept;
	~SessionHandle();

	// Drives the TLS handshake (non-blocking).
	[[nodiscard]] HandshakeResult handshake() const;

	// Certificate validation result after a handshake attempt (only
	// meaningful when the session was created with verify=true; valid
	// whether the handshake itself failed or not). Returns 0 when the
	// certificate is valid, otherwise a TCP-IP UNAPI close reason code
	// in the range 9..19 (spec 4.5.4). The code is purely informational
	// - the connection is refused either way - so a non-zero value
	// requires no special handling.
	[[nodiscard]] int verifyResult() const;

	// Human-readable description of the certificate verification result
	// (e.g. "self-signed certificate", "hostname mismatch"); empty when
	// the certificate verified OK. Valid after any handshake attempt.
	[[nodiscard]] std::string verifyErrorDescription() const;

	// Decrypts inbound data. Success: number of bytes read. Failure:
	// WouldBlock, or Closed on a clean TLS close (close_notify), or
	// Failed with the OpenSSL error code.
	[[nodiscard]] IoResult read(std::span<char> buf) const;

	// Encrypts and sends data. Success: number of bytes written. Failure:
	// WouldBlock, or Closed when the peer sent close_notify, or Failed
	// with the OpenSSL error code.
	[[nodiscard]] IoResult write(std::span<const char> buf) const;

	// Plaintext bytes already decrypted and buffered by the SSL layer,
	// readable without waiting for socket readiness.
	[[nodiscard]] size_t pending() const;

private:
	friend struct LibHandle;
	SessionHandle(Ssl* ssl_, const OpenSSLApi* api_);
	void release() noexcept;
	Ssl* ssl;
	const OpenSSLApi* api;
};

// Handle to the loaded OpenSSL runtime. Obtained from load(); its members
// may only be called after load() succeeded.
struct LibHandle {
	// Version string, e.g. "OpenSSL 3.1.4 24 Oct 2023".
	[[nodiscard]] zstring_view version() const;

	// Creates a TLS client session on the given connected (non-blocking)
	// socket.
	// - verify: validate the server certificate (the TCP-IP UNAPI spec
	//   "Verify the server certificate" flag). When false, no validation
	//   at all is performed.
	// - hostname: server name, sent as SNI and (when verify is set) used
	//   to check the certificate host name. May be empty.
	// Returns a session handle, or nullopt on failure.
	[[nodiscard]] std::optional<SessionHandle> createClientSession(
		bool verify, zstring_view hostname, int fd) const;

private:
	friend LibHandle* load();
	friend std::string to_string(ErrorCode ec);
	LibHandle();
	~LibHandle();
	std::unique_ptr<OpenSSLApi> api; // resolved entry points
	void* libHandle = nullptr;       // HMODULE / dlopen handle
	void* cryptoHandle = nullptr;    // matching libcrypto handle
	bool loaded = false;
};

// Loads the runtime library and resolves the needed entry points.
// Idempotent and thread-safe (the library is loaded at most once, also
// when called concurrently from several threads). Returns nullptr when
// OpenSSL is not available (the returned pointer is the same on every
// successful call).
LibHandle* load();

} // namespace openmsx::OpenSSL

#endif