#!/bin/bash
# Generate build_info.h with git commit hash and build timestamp

OUTPUT_FILE="$1"

# Get git commit hash (short)
GIT_HASH=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")

# Get git commit hash (full)
GIT_HASH_FULL=$(git rev-parse HEAD 2>/dev/null || echo "unknown")

# Get git branch
GIT_BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")

# Check if working directory is clean
if git diff-index --quiet HEAD -- 2>/dev/null; then
    GIT_DIRTY="false"
else
    GIT_DIRTY="true"
fi

# Get build timestamp (ISO 8601 format)
BUILD_TIME=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# Get build timestamp (Unix epoch)
BUILD_TIMESTAMP=$(date +%s)

# Get ESP-IDF version
IDF_VERSION=$(git -C "${IDF_PATH:-esp-idf}" describe --tags 2>/dev/null || echo "unknown")

# Get project version from latest git tag (e.g., v2.0.0 -> 2.0.0)
# Falls back to "dev" if no tags exist
GIT_VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || echo "dev")
# If we're ahead of the tag, include commit count (e.g., 2.0.0-5-g1234abc -> 2.0.0+5)
VERSION_AHEAD=$(git describe --tags 2>/dev/null | sed 's/^v//' | grep -o '\-[0-9]*\-' | tr -d '-' || echo "")
if [ -n "$VERSION_AHEAD" ] && [ "$VERSION_AHEAD" != "0" ]; then
    BUILD_VERSION="${GIT_VERSION}+${VERSION_AHEAD}"
else
    BUILD_VERSION="$GIT_VERSION"
fi

# Generate header file
cat > "$OUTPUT_FILE" << EOF
/**
 * @file build_info.h
 * @brief Auto-generated build information
 *
 * This file is generated at build time by generate_build_info.sh
 * DO NOT EDIT MANUALLY - it will be overwritten
 */

#ifndef BUILD_INFO_H
#define BUILD_INFO_H

// Git information
#define BUILD_GIT_HASH       "$GIT_HASH"
#define BUILD_GIT_HASH_FULL  "$GIT_HASH_FULL"
#define BUILD_GIT_BRANCH     "$GIT_BRANCH"
#define BUILD_GIT_DIRTY      $GIT_DIRTY

// Build timestamp
#define BUILD_TIME           "$BUILD_TIME"
#define BUILD_TIMESTAMP      ${BUILD_TIMESTAMP}UL

// ESP-IDF version
#define BUILD_IDF_VERSION    "$IDF_VERSION"

// Build fingerprint (short identifier for this build)
#define BUILD_FINGERPRINT    "$GIT_HASH"

// Project version (from git tags, e.g., "2.0.0" or "2.0.0+5" if ahead of tag)
#define BUILD_VERSION        "$BUILD_VERSION"

#endif /* BUILD_INFO_H */
EOF

echo "Generated $OUTPUT_FILE:"
echo "  Git Hash: $GIT_HASH"
echo "  Git Branch: $GIT_BRANCH"
echo "  Dirty: $GIT_DIRTY"
echo "  Build Time: $BUILD_TIME"
echo "  Version: $BUILD_VERSION"
