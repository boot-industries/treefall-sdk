// SPDX-License-Identifier: MIT
#pragma once

#include <orpheus/export.h>

#include <cstdint>
#include <optional>
#include <string>

namespace orpheus {

inline constexpr uint32_t kMediaFingerprintSchemaVersion = 1;
inline constexpr const char* kMediaFingerprintAlgorithmSha256 = "sha256";

/// Versioned digest stored with a media reference.
struct MediaFingerprint {
  uint32_t schemaVersion = kMediaFingerprintSchemaVersion;
  std::string algorithm = kMediaFingerprintAlgorithmSha256;
  std::string digestHex;
};

enum class MediaVerificationPolicy : uint8_t {
  Optional = 0,
  Required = 1,
};

/// Portable media identity. Paths are relative to a caller-supplied media root.
struct MediaReference {
  std::string relativePath;
  std::optional<MediaFingerprint> fingerprint;
  MediaVerificationPolicy verification = MediaVerificationPolicy::Optional;
};

enum class FileHashStatus : uint8_t {
  OK = 0,
  NotFound,
  ReadError,
  ProviderError,
};

struct FileHashResult {
  FileHashStatus status = FileHashStatus::ReadError;
  std::string digestHex;
  uint64_t bytesRead = 0;
  std::string message;

  [[nodiscard]] bool isOK() const noexcept {
    return status == FileHashStatus::OK;
  }
};

enum class MediaResolutionStatus : uint8_t {
  Verified = 0,
  ResolvedUnverified,
  Missing,
  HashMismatch,
  InvalidReference,
  ReadError,
};

struct MediaResolutionResult {
  MediaResolutionStatus status = MediaResolutionStatus::InvalidReference;
  std::string resolvedPath;
  std::optional<MediaFingerprint> observedFingerprint;
  std::string message;

  /// Safe default for playback. A mismatch is never silently usable; a host
  /// must implement any explicit user override outside this resolver.
  [[nodiscard]] bool isUsable() const noexcept {
    return status == MediaResolutionStatus::Verified ||
           status == MediaResolutionStatus::ResolvedUnverified;
  }
};

/// Stream a file through the platform's vetted SHA-256 provider. Background
/// thread only: this performs file I/O and cryptographic work.
ORPHEUS_API FileHashResult sha256File(const std::string& filePath) noexcept;

/// Resolve and, when present, verify a media reference beneath mediaRoot.
/// Absolute paths and parent traversal are rejected deterministically.
ORPHEUS_API MediaResolutionResult resolveMediaReference(const std::string& mediaRoot,
                                                        const MediaReference& reference) noexcept;

} // namespace orpheus
