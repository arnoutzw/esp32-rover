#!/bin/bash
# ESP32 Rover Release Script
# Automates version bumping, changelog generation, and release creation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_usage() {
    echo ""
    echo "ESP32 Rover Release Script"
    echo "=========================="
    echo ""
    echo "Usage: $0 <bump-type> [release-message]"
    echo ""
    echo "Bump types:"
    echo "  major     Bump major version (X.0.0) - Breaking changes"
    echo "  minor     Bump minor version (x.Y.0) - New features"
    echo "  patch     Bump patch version (x.y.Z) - Bug fixes"
    echo ""
    echo "Options:"
    echo "  --dry-run   Show what would be done without making changes"
    echo "  --no-build  Skip building binaries (use existing)"
    echo "  -h, --help  Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 patch \"Bug fix for OTA stability\""
    echo "  $0 minor \"Added MQTT telemetry support\""
    echo "  $0 major \"Complete architecture rewrite\""
    echo "  $0 patch --dry-run"
    echo ""
    echo "This script will:"
    echo "  1. Calculate the new version number"
    echo "  2. Generate changelog entry from commits"
    echo "  3. Build both targets (esp32cam, ttgo)"
    echo "  4. Create release directory with binaries"
    echo "  5. Merge develop to main"
    echo "  6. Create and push git tag"
    echo "  7. Return to develop branch"
    echo ""
}

get_current_version() {
    git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0"
}

calculate_new_version() {
    local current=$1
    local bump_type=$2

    # Remove 'v' prefix
    local version=${current#v}

    # Split into components
    local major=$(echo "$version" | cut -d. -f1)
    local minor=$(echo "$version" | cut -d. -f2)
    local patch=$(echo "$version" | cut -d. -f3)

    case $bump_type in
        major)
            major=$((major + 1))
            minor=0
            patch=0
            ;;
        minor)
            minor=$((minor + 1))
            patch=0
            ;;
        patch)
            patch=$((patch + 1))
            ;;
    esac

    echo "v${major}.${minor}.${patch}"
}

generate_changelog_entry() {
    local current_tag=$1
    local new_tag=$2
    local date=$(date +"%Y-%m-%d")

    echo "## [$new_tag] - $date"
    echo ""

    # Group commits by type
    local features=$(git log ${current_tag}..HEAD --pretty=format:"- %s" --grep="^feat" --grep="^add" -i 2>/dev/null || true)
    local fixes=$(git log ${current_tag}..HEAD --pretty=format:"- %s" --grep="^fix" -i 2>/dev/null || true)
    local other=$(git log ${current_tag}..HEAD --pretty=format:"- %s" 2>/dev/null | grep -v -i "^- feat" | grep -v -i "^- fix" | grep -v -i "^- add" || true)

    if [ -n "$features" ]; then
        echo "### Added"
        echo "$features"
        echo ""
    fi

    if [ -n "$fixes" ]; then
        echo "### Fixed"
        echo "$fixes"
        echo ""
    fi

    if [ -n "$other" ]; then
        echo "### Changed"
        echo "$other"
        echo ""
    fi
}

# Parse arguments
DRY_RUN=0
NO_BUILD=0
BUMP_TYPE=""
MESSAGE=""

while [ $# -gt 0 ]; do
    case $1 in
        major|minor|patch)
            BUMP_TYPE=$1
            shift
            ;;
        --dry-run)
            DRY_RUN=1
            shift
            ;;
        --no-build)
            NO_BUILD=1
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            if [ -z "$MESSAGE" ]; then
                MESSAGE="$1"
            fi
            shift
            ;;
    esac
done

if [ -z "$BUMP_TYPE" ]; then
    echo -e "${RED}Error: Bump type required (major, minor, or patch)${NC}"
    print_usage
    exit 1
fi

cd "$PROJECT_ROOT"

# Check we're on develop branch
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
if [ "$CURRENT_BRANCH" != "develop" ]; then
    echo -e "${RED}Error: Must be on develop branch to create a release${NC}"
    echo "Current branch: $CURRENT_BRANCH"
    echo "Run: git checkout develop"
    exit 1
fi

# Check for uncommitted changes
if [ -n "$(git status --porcelain)" ]; then
    echo -e "${RED}Error: Working directory has uncommitted changes${NC}"
    echo "Commit or stash changes before releasing"
    git status --short
    exit 1
fi

# Calculate versions
CURRENT_VERSION=$(get_current_version)
NEW_VERSION=$(calculate_new_version "$CURRENT_VERSION" "$BUMP_TYPE")
MESSAGE="${MESSAGE:-Release $NEW_VERSION}"

echo ""
echo "ESP32 Rover Release"
echo "==================="
echo ""
echo -e "Current version: ${YELLOW}$CURRENT_VERSION${NC}"
echo -e "New version:     ${GREEN}$NEW_VERSION${NC}"
echo -e "Bump type:       $BUMP_TYPE"
echo -e "Message:         $MESSAGE"
echo ""

if [ $DRY_RUN -eq 1 ]; then
    echo -e "${YELLOW}=== DRY RUN - No changes will be made ===${NC}"
    echo ""
fi

# Generate changelog entry
echo -e "${BLUE}Generating changelog entry...${NC}"
CHANGELOG_ENTRY=$(generate_changelog_entry "$CURRENT_VERSION" "$NEW_VERSION")
echo ""
echo "$CHANGELOG_ENTRY"
echo ""

if [ $DRY_RUN -eq 1 ]; then
    echo -e "${YELLOW}Would build both targets and create release${NC}"
    exit 0
fi

# Confirm
read -p "Proceed with release? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Release cancelled"
    exit 1
fi

# Build both targets
if [ $NO_BUILD -eq 0 ]; then
    echo ""
    echo -e "${BLUE}Building ESP32-CAM...${NC}"
    ALLOW_DIRTY=1 ./scripts/build.sh esp32cam build

    echo ""
    echo -e "${BLUE}Building TTGO...${NC}"
    ALLOW_DIRTY=1 ./scripts/build.sh ttgo build
fi

# Create release directory
RELEASE_DIR="$PROJECT_ROOT/releases/$NEW_VERSION"
mkdir -p "$RELEASE_DIR"

# Copy binaries
echo ""
echo -e "${BLUE}Copying binaries to release directory...${NC}"
cp "$PROJECT_ROOT/binaries/esp32cam/latest.bin" "$RELEASE_DIR/esp32-rover-esp32cam-${NEW_VERSION}.bin"
cp "$PROJECT_ROOT/binaries/ttgo/latest.bin" "$RELEASE_DIR/esp32-rover-ttgo-${NEW_VERSION}.bin"

# Generate checksums
cd "$RELEASE_DIR"
shasum -a 256 *.bin > SHA256SUMS.txt
cd "$PROJECT_ROOT"

# Save changelog entry
echo "$CHANGELOG_ENTRY" > "$RELEASE_DIR/RELEASE_NOTES.md"

# Update main CHANGELOG.md
if [ -f "CHANGELOG.md" ]; then
    # Insert new entry after the header
    TEMP_FILE=$(mktemp)
    head -n 6 CHANGELOG.md > "$TEMP_FILE"
    echo "" >> "$TEMP_FILE"
    echo "$CHANGELOG_ENTRY" >> "$TEMP_FILE"
    tail -n +7 CHANGELOG.md >> "$TEMP_FILE"
    mv "$TEMP_FILE" CHANGELOG.md
fi

# Commit changelog updates
git add CHANGELOG.md releases/
git commit -m "Release $NEW_VERSION" || true

# Merge to main and tag
echo ""
echo -e "${BLUE}Merging to main branch...${NC}"
git checkout main
git merge develop -m "Merge develop for release $NEW_VERSION"

echo ""
echo -e "${BLUE}Creating tag $NEW_VERSION...${NC}"
git tag -a "$NEW_VERSION" -m "$MESSAGE"

# Push
echo ""
echo -e "${BLUE}Pushing to origin...${NC}"
git push origin main
git push origin "$NEW_VERSION"

# Return to develop
git checkout develop

echo ""
echo -e "${GREEN}=== Release $NEW_VERSION complete! ===${NC}"
echo ""
echo "Release artifacts:"
ls -la "$RELEASE_DIR"
echo ""
echo "GitHub release URL (create manually or via gh CLI):"
echo "  https://github.com/YOUR_ORG/esp32-rover-firmware/releases/new?tag=$NEW_VERSION"
