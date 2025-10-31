#!/usr/bin/env bash
# Skill: Build Utilities
# Purpose: Helper functions for building and packaging Clip Composer
# Usage: source .claude/skills/build-utils.sh
# Dependencies: cmake, hdiutil (macOS)

# Configuration
readonly SDK_ROOT="${SDK_ROOT:-/Users/chrislyons/dev/orpheus-sdk}"
readonly BUILD_DIR="${BUILD_DIR:-$SDK_ROOT/build}"
readonly APP_NAME="OrpheusClipComposer"

# Colors
readonly RED='\033[0;31m'
readonly GREEN='\033[0;32m'
readonly YELLOW='\033[1;33m'
readonly BLUE='\033[0;34m'
readonly NC='\033[0m'

# Helper functions
info() { echo -e "${BLUE}ℹ${NC} $1"; }
success() { echo -e "${GREEN}✓${NC} $1"; }
warn() { echo -e "${YELLOW}⚠${NC} $1"; }
error() { echo -e "${RED}✗${NC} $1"; }

# Build Debug configuration
occ_build_debug() {
    info "Building Clip Composer (Debug)..."

    cd "$SDK_ROOT" || return 1

    cmake -S . -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_OSX_ARCHITECTURES="arm64" \
        -DORPHEUS_ENABLE_APP_CLIP_COMPOSER=ON

    if ! cmake --build "$BUILD_DIR" --target orpheus_clip_composer_app; then
        error "Build failed"
        return 1
    fi

    success "Build completed successfully"
    return 0
}

# Build Release configuration (when linker issues fixed)
occ_build_release() {
    warn "Release builds currently have linker issues (as of v0.1.0-alpha)"
    warn "Falling back to Debug build..."

    occ_build_debug
    return $?
}

# Verify binary architecture and size
occ_verify_binary() {
    local build_type="${1:-Debug}"
    local app_path="$BUILD_DIR/apps/clip-composer/orpheus_clip_composer_app_artefacts/$build_type/$APP_NAME.app"

    if [[ ! -d "$app_path" ]]; then
        error "Application bundle not found at: $app_path"
        return 1
    fi

    success "Application bundle found"

    # Check binary
    local binary="$app_path/Contents/MacOS/$APP_NAME"
    if [[ ! -f "$binary" ]]; then
        error "Binary not found: $binary"
        return 1
    fi

    # Architecture
    info "Architecture:"
    lipo -info "$binary" | sed 's/^/  /'

    # Size
    local size=$(du -sh "$app_path" | cut -f1)
    info "Bundle size: $size"

    # Executable check
    if [[ -x "$binary" ]]; then
        success "Binary is executable"
    else
        warn "Binary is not executable"
    fi

    return 0
}

# Create DMG package
occ_create_dmg() {
    local version="${1:-v0.1.0-alpha}"
    local arch="${2:-arm64}"
    local build_type="${3:-Debug}"

    info "Creating DMG for $version ($arch, $build_type)..."

    local app_path="$BUILD_DIR/apps/clip-composer/orpheus_clip_composer_app_artefacts/$build_type/$APP_NAME.app"

    # Verify binary exists
    if [[ ! -d "$app_path" ]]; then
        error "Application bundle not found. Build first with occ_build_debug"
        return 1
    fi

    # Create staging directory
    local staging="/tmp/occ-staging"
    rm -rf "$staging"
    mkdir -p "$staging"

    info "Staging application..."
    cp -R "$app_path" "$staging/"

    # Create DMG
    local dmg_name="${APP_NAME}-${version}-${arch}.dmg"
    info "Creating $dmg_name..."

    if ! hdiutil create \
        -volname "Orpheus Clip Composer" \
        -srcfolder "$staging" \
        -ov \
        -format UDZO \
        "$dmg_name"; then
        error "DMG creation failed"
        rm -rf "$staging"
        return 1
    fi

    # Clean up
    rm -rf "$staging"

    # Verify DMG
    local dmg_size=$(du -sh "$dmg_name" | cut -f1)
    success "DMG created: $dmg_name ($dmg_size)"

    info "DMG info:"
    hdiutil imageinfo "$dmg_name" | grep -E "(Format|Size)" | sed 's/^/  /'

    return 0
}

# Launch application for testing
occ_launch() {
    local build_type="${1:-Debug}"
    local app_path="$BUILD_DIR/apps/clip-composer/orpheus_clip_composer_app_artefacts/$build_type/$APP_NAME.app"

    if [[ ! -d "$app_path" ]]; then
        error "Application not found. Build first with occ_build_debug"
        return 1
    fi

    info "Launching $APP_NAME..."
    open "$app_path"
    return 0
}

# Clean build artifacts
occ_clean() {
    local clean_all="${1:-false}"

    if [[ "$clean_all" == "true" ]]; then
        warn "Cleaning entire build directory..."
        rm -rf "$BUILD_DIR"
        success "Build directory removed"
    else
        info "Cleaning Clip Composer build artifacts..."
        rm -rf "$BUILD_DIR/apps/clip-composer"
        success "Clip Composer artifacts removed"
    fi

    return 0
}

# Show help
occ_help() {
    cat <<EOF
Orpheus Clip Composer - Build Utilities
========================================

Available functions:

  occ_build_debug            Build Debug configuration
  occ_build_release          Build Release configuration (currently falls back to Debug)
  occ_verify_binary [type]   Verify binary architecture and size (default: Debug)
  occ_create_dmg [ver] [arch] [type]
                             Create DMG package (default: v0.1.0-alpha, arm64, Debug)
  occ_launch [type]          Launch application for testing (default: Debug)
  occ_clean [all]            Clean build artifacts (all=true to remove entire build dir)
  occ_help                   Show this help message

Configuration:
  SDK_ROOT:   $SDK_ROOT
  BUILD_DIR:  $BUILD_DIR
  APP_NAME:   $APP_NAME

Example workflow:
  source .claude/skills/build-utils.sh
  occ_build_debug
  occ_verify_binary
  occ_launch
  occ_create_dmg "v0.2.0-beta"

EOF
}

# If sourced, just define functions
# If executed directly, show help
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    occ_help
fi
