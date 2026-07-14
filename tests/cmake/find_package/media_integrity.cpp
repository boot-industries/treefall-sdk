// SPDX-License-Identifier: MIT
#include <orpheus/media_integrity.h>

#include <filesystem>
#include <fstream>

int main() {
  const auto root = std::filesystem::temp_directory_path() / "orpheus_installed_media_fixture";
  std::filesystem::create_directories(root);
  const auto path = root / "media.bin";
  {
    std::ofstream file(path, std::ios::binary);
    file << "abc";
  }

  const auto hash = orpheus::sha256File(path.string());
  orpheus::MediaReference reference;
  reference.relativePath = "media.bin";
  reference.verification = orpheus::MediaVerificationPolicy::Required;
  reference.fingerprint = orpheus::MediaFingerprint{1, "sha256", hash.digestHex};
  const auto resolved = orpheus::resolveMediaReference(root.string(), reference);
  std::filesystem::remove_all(root);
  return hash.isOK() && resolved.status == orpheus::MediaResolutionStatus::Verified ? 0 : 1;
}
