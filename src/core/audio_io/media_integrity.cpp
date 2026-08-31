// SPDX-License-Identifier: MIT
#include <orpheus/media_integrity.h>

#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

#if defined(__APPLE__)
#include <CommonCrypto/CommonDigest.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <bcrypt.h>
#include <windows.h>
#else
#include <openssl/evp.h>
#endif

namespace orpheus {
namespace {

constexpr size_t kReadBufferBytes = 64 * 1024;
constexpr size_t kSha256Bytes = 32;

std::string toHex(const std::array<unsigned char, kSha256Bytes>& digest) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string result(kSha256Bytes * 2, '0');
  for (size_t i = 0; i < digest.size(); ++i) {
    result[i * 2] = kHex[digest[i] >> 4];
    result[i * 2 + 1] = kHex[digest[i] & 0x0f];
  }
  return result;
}

bool validDigest(const std::string& digest) {
  if (digest.size() != kSha256Bytes * 2) {
    return false;
  }
  for (const char character : digest) {
    const auto c = static_cast<unsigned char>(character);
    if (std::isxdigit(c) == 0) {
      return false;
    }
  }
  return true;
}

std::string lowerHex(std::string digest) {
  for (char& c : digest) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return digest;
}

bool safeRelativePath(const std::filesystem::path& path) {
  if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
    return false;
  }
  for (const auto& component : path) {
    if (component == "..") {
      return false;
    }
  }
  return true;
}

#if defined(__APPLE__)
class Sha256Provider {
public:
  bool initialize() {
    return CC_SHA256_Init(&context_) == 1;
  }
  bool update(const unsigned char* data, size_t size) {
    return CC_SHA256_Update(&context_, data, static_cast<CC_LONG>(size)) == 1;
  }
  bool finish(std::array<unsigned char, kSha256Bytes>& digest) {
    return CC_SHA256_Final(digest.data(), &context_) == 1;
  }

private:
  CC_SHA256_CTX context_{};
};
#elif defined(_WIN32)
class Sha256Provider {
public:
  ~Sha256Provider() {
    if (hash_ != nullptr) {
      BCryptDestroyHash(hash_);
    }
    if (algorithm_ != nullptr) {
      BCryptCloseAlgorithmProvider(algorithm_, 0);
    }
  }

  bool initialize() {
    return BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
           BCryptCreateHash(algorithm_, &hash_, nullptr, 0, nullptr, 0, 0) >= 0;
  }
  bool update(const unsigned char* data, size_t size) {
    return BCryptHashData(hash_, const_cast<PUCHAR>(data), static_cast<ULONG>(size), 0) >= 0;
  }
  bool finish(std::array<unsigned char, kSha256Bytes>& digest) {
    return BCryptFinishHash(hash_, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
  }

private:
  BCRYPT_ALG_HANDLE algorithm_ = nullptr;
  BCRYPT_HASH_HANDLE hash_ = nullptr;
};
#else
class Sha256Provider {
public:
  ~Sha256Provider() {
    EVP_MD_CTX_free(context_);
  }

  bool initialize() {
    context_ = EVP_MD_CTX_new();
    return context_ != nullptr && EVP_DigestInit_ex(context_, EVP_sha256(), nullptr) == 1;
  }
  bool update(const unsigned char* data, size_t size) {
    return EVP_DigestUpdate(context_, data, size) == 1;
  }
  bool finish(std::array<unsigned char, kSha256Bytes>& digest) {
    unsigned int length = 0;
    return EVP_DigestFinal_ex(context_, digest.data(), &length) == 1 && length == digest.size();
  }

private:
  EVP_MD_CTX* context_ = nullptr;
};
#endif

} // namespace

FileHashResult sha256File(const std::string& filePath) noexcept {
  try {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
      std::error_code error;
      const bool exists = std::filesystem::exists(filePath, error);
      return {exists ? FileHashStatus::ReadError : FileHashStatus::NotFound,
              {},
              0,
              exists ? "Unable to open media for hashing" : "Media file not found"};
    }

    Sha256Provider provider;
    if (!provider.initialize()) {
      return {FileHashStatus::ProviderError, {}, 0, "Unable to initialize SHA-256 provider"};
    }

    std::array<char, kReadBufferBytes> buffer{};
    uint64_t bytesRead = 0;
    while (file) {
      file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
      const std::streamsize count = file.gcount();
      if (count > 0) {
        if (!provider.update(reinterpret_cast<const unsigned char*>(buffer.data()),
                             static_cast<size_t>(count))) {
          return {FileHashStatus::ProviderError,
                  {},
                  bytesRead,
                  "SHA-256 provider rejected media bytes"};
        }
        bytesRead += static_cast<uint64_t>(count);
      }
    }
    if (!file.eof()) {
      return {FileHashStatus::ReadError, {}, bytesRead, "Failed while reading media"};
    }

    std::array<unsigned char, kSha256Bytes> digest{};
    if (!provider.finish(digest)) {
      return {FileHashStatus::ProviderError, {}, bytesRead, "Unable to finalize SHA-256 digest"};
    }
    return {FileHashStatus::OK, toHex(digest), bytesRead, {}};
  } catch (const std::exception& error) {
    return {FileHashStatus::ReadError, {}, 0, error.what()};
  } catch (...) {
    return {FileHashStatus::ReadError, {}, 0, "Unknown media hashing failure"};
  }
}

MediaResolutionResult resolveMediaReference(const std::string& mediaRoot,
                                            const MediaReference& reference) noexcept {
  try {
    const std::filesystem::path relative(reference.relativePath);
    if (!safeRelativePath(relative)) {
      return {MediaResolutionStatus::InvalidReference,
              {},
              std::nullopt,
              "Media path must be a non-empty relative path without parent traversal"};
    }
    if (reference.verification == MediaVerificationPolicy::Required &&
        !reference.fingerprint.has_value()) {
      return {MediaResolutionStatus::InvalidReference,
              {},
              std::nullopt,
              "Required media verification has no fingerprint"};
    }

    const std::filesystem::path resolved =
        (std::filesystem::path(mediaRoot) / relative).lexically_normal();
    std::error_code error;
    if (!std::filesystem::is_regular_file(resolved, error)) {
      return {MediaResolutionStatus::Missing, resolved.string(), std::nullopt,
              "Referenced media file is missing"};
    }

    if (!reference.fingerprint.has_value()) {
      return {MediaResolutionStatus::ResolvedUnverified, resolved.string(), std::nullopt, {}};
    }

    const MediaFingerprint& expected = *reference.fingerprint;
    if (expected.schemaVersion != kMediaFingerprintSchemaVersion ||
        expected.algorithm != kMediaFingerprintAlgorithmSha256 ||
        !validDigest(expected.digestHex)) {
      return {MediaResolutionStatus::InvalidReference, resolved.string(), std::nullopt,
              "Unsupported or malformed media fingerprint"};
    }

    const FileHashResult hash = sha256File(resolved.string());
    if (!hash.isOK()) {
      return {hash.status == FileHashStatus::NotFound ? MediaResolutionStatus::Missing
                                                      : MediaResolutionStatus::ReadError,
              resolved.string(), std::nullopt, hash.message};
    }

    MediaFingerprint observed;
    observed.digestHex = hash.digestHex;
    if (hash.digestHex != lowerHex(expected.digestHex)) {
      return {MediaResolutionStatus::HashMismatch, resolved.string(), observed,
              "Media SHA-256 does not match the reference"};
    }
    return {MediaResolutionStatus::Verified, resolved.string(), observed, {}};
  } catch (const std::exception& error) {
    return {MediaResolutionStatus::ReadError, {}, std::nullopt, error.what()};
  } catch (...) {
    return {MediaResolutionStatus::ReadError, {}, std::nullopt, "Unknown media resolution failure"};
  }
}

} // namespace orpheus
