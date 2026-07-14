// SPDX-License-Identifier: MIT
#include <orpheus/media_integrity.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace orpheus {
namespace {

class MediaIntegrityTest : public ::testing::Test {
protected:
  void SetUp() override {
    root_ = std::filesystem::temp_directory_path() / "orpheus_media_integrity_test";
    std::filesystem::remove_all(root_);
    std::filesystem::create_directories(root_);
  }

  void TearDown() override {
    std::filesystem::remove_all(root_);
  }

  std::filesystem::path writeFile(const std::string& name, const std::string& contents) {
    const auto path = root_ / name;
    std::ofstream file(path, std::ios::binary);
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();
    return path;
  }

  std::filesystem::path root_;
};

TEST_F(MediaIntegrityTest, Sha256MatchesPublishedAbcVector) {
  const auto path = writeFile("abc.bin", "abc");
  const FileHashResult hash = sha256File(path.string());
  ASSERT_TRUE(hash.isOK()) << hash.message;
  EXPECT_EQ(hash.digestHex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(hash.bytesRead, 3u);
}

TEST_F(MediaIntegrityTest, Sha256StreamsAcrossReadBoundaries) {
  const std::string contents(200000, 'a');
  const auto path = writeFile("large.bin", contents);
  const FileHashResult hash = sha256File(path.string());
  ASSERT_TRUE(hash.isOK()) << hash.message;
  EXPECT_EQ(hash.digestHex, "2287d207f24a941ff3b56c04c8a25ad56b63e3023207b3bb5b4ac0c9869d74be");
  EXPECT_EQ(hash.bytesRead, contents.size());
}

TEST_F(MediaIntegrityTest, ResolveReportsVerifiedMissingAndMismatch) {
  const auto path = writeFile("take.wav", "abc");
  const FileHashResult hash = sha256File(path.string());
  ASSERT_TRUE(hash.isOK());

  MediaReference reference;
  reference.relativePath = "take.wav";
  reference.verification = MediaVerificationPolicy::Required;
  reference.fingerprint = MediaFingerprint{1, "sha256", hash.digestHex};

  const auto verified = resolveMediaReference(root_.string(), reference);
  EXPECT_EQ(verified.status, MediaResolutionStatus::Verified);
  EXPECT_TRUE(verified.isUsable());

  reference.fingerprint->digestHex.assign(64, '0');
  const auto mismatch = resolveMediaReference(root_.string(), reference);
  EXPECT_EQ(mismatch.status, MediaResolutionStatus::HashMismatch);
  EXPECT_FALSE(mismatch.isUsable());
  ASSERT_TRUE(mismatch.observedFingerprint.has_value());
  EXPECT_EQ(mismatch.observedFingerprint->digestHex, hash.digestHex);

  reference.relativePath = "missing.wav";
  const auto missing = resolveMediaReference(root_.string(), reference);
  EXPECT_EQ(missing.status, MediaResolutionStatus::Missing);
  EXPECT_FALSE(missing.isUsable());
}

TEST_F(MediaIntegrityTest, ResolveRejectsUnsafeAndUnverifiableRequiredReferences) {
  MediaReference traversal;
  traversal.relativePath = "../outside.wav";
  EXPECT_EQ(resolveMediaReference(root_.string(), traversal).status,
            MediaResolutionStatus::InvalidReference);

  MediaReference required;
  required.relativePath = "take.wav";
  required.verification = MediaVerificationPolicy::Required;
  EXPECT_EQ(resolveMediaReference(root_.string(), required).status,
            MediaResolutionStatus::InvalidReference);
}

TEST_F(MediaIntegrityTest, ResolveMakesUnverifiedStateExplicit) {
  writeFile("take.wav", "abc");
  MediaReference reference;
  reference.relativePath = "take.wav";
  const auto result = resolveMediaReference(root_.string(), reference);
  EXPECT_EQ(result.status, MediaResolutionStatus::ResolvedUnverified);
  EXPECT_TRUE(result.isUsable());
}

} // namespace
} // namespace orpheus
