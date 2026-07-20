// SPDX-License-Identifier: MIT
//
// Ensures CMake's Xcode generator emits the `orpheus_diagnostics` archive.
// The archive otherwise consists only of object-library generator expressions,
// which Xcode recognizes for link flags but does not materialize as a product.

namespace orpheus::detail {
void diagnosticsArchiveAnchor() noexcept {}
} // namespace orpheus::detail
