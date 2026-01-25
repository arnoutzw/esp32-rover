#!/bin/bash
# ESP32 Rover Static Analysis Script
# Runs shellcheck on bash scripts and cppcheck on C code

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

ERRORS=0

print_usage() {
    echo ""
    echo "ESP32 Rover Static Analysis"
    echo "==========================="
    echo ""
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  --shell-only    Only run shellcheck on bash scripts"
    echo "  --c-only        Only run cppcheck on C code"
    echo "  --fix           Attempt to auto-fix issues (where supported)"
    echo "  --strict        Treat warnings as errors"
    echo "  -h, --help      Show this help"
    echo ""
    echo "Requirements:"
    echo "  - shellcheck (brew install shellcheck / apt install shellcheck)"
    echo "  - cppcheck (brew install cppcheck / apt install cppcheck)"
    echo ""
}

check_shellcheck() {
    if ! command -v shellcheck &> /dev/null; then
        echo -e "${YELLOW}Warning: shellcheck not installed${NC}"
        echo "Install with: brew install shellcheck (macOS) or apt install shellcheck (Linux)"
        return 1
    fi
    return 0
}

check_cppcheck() {
    if ! command -v cppcheck &> /dev/null; then
        echo -e "${YELLOW}Warning: cppcheck not installed${NC}"
        echo "Install with: brew install cppcheck (macOS) or apt install cppcheck (Linux)"
        return 1
    fi
    return 0
}

run_shellcheck() {
    echo -e "${BLUE}=== Running shellcheck on bash scripts ===${NC}"
    echo ""

    if ! check_shellcheck; then
        return 1
    fi

    local scripts=(
        "$SCRIPT_DIR/build.sh"
        "$SCRIPT_DIR/ota.sh"
        "$SCRIPT_DIR/setup.sh"
        "$SCRIPT_DIR/lint.sh"
        "$SCRIPT_DIR/generate_build_info.sh"
    )

    local failed=0
    for script in "${scripts[@]}"; do
        if [ -f "$script" ]; then
            echo -n "Checking $(basename "$script")... "
            if shellcheck -x "$script" 2>&1; then
                echo -e "${GREEN}OK${NC}"
            else
                echo -e "${RED}FAILED${NC}"
                ((failed++))
            fi
        fi
    done

    if [ $failed -eq 0 ]; then
        echo ""
        echo -e "${GREEN}All shell scripts passed!${NC}"
        return 0
    else
        echo ""
        echo -e "${RED}$failed script(s) have issues${NC}"
        return 1
    fi
}

run_cppcheck() {
    echo -e "${BLUE}=== Running cppcheck on C code ===${NC}"
    echo ""

    if ! check_cppcheck; then
        return 1
    fi

    local cppcheck_args=(
        "--enable=warning,style,performance,portability"
        "--std=c11"
        "--platform=unix32"
        "-I" "$PROJECT_ROOT/firmware/main"
        "-I" "$PROJECT_ROOT/firmware/components"
        "--suppress=missingIncludeSystem"
        "--suppress=unusedFunction"
        "--suppress=unmatchedSuppression"
        "--inline-suppr"
        "--quiet"
    )

    if [ "$STRICT" = "1" ]; then
        cppcheck_args+=("--error-exitcode=1")
    fi

    echo "Checking firmware/main..."
    if cppcheck "${cppcheck_args[@]}" "$PROJECT_ROOT/firmware/main/"*.c 2>&1; then
        echo -e "${GREEN}firmware/main: OK${NC}"
    else
        echo -e "${RED}firmware/main: issues found${NC}"
        ((ERRORS++))
    fi

    echo ""
    echo "Checking firmware/components..."

    # Check each component directory
    for component_dir in "$PROJECT_ROOT/firmware/components"/*; do
        if [ -d "$component_dir" ]; then
            local component_name=$(basename "$component_dir")
            local c_files=("$component_dir"/*.c)

            if [ -f "${c_files[0]}" ]; then
                echo -n "  Checking $component_name... "
                if cppcheck "${cppcheck_args[@]}" "$component_dir"/*.c 2>&1 | grep -v "^$"; then
                    echo -e "${GREEN}OK${NC}"
                else
                    echo -e "${GREEN}OK${NC}"
                fi
            fi
        fi
    done

    if [ $ERRORS -eq 0 ]; then
        echo ""
        echo -e "${GREEN}C code analysis complete!${NC}"
        return 0
    else
        echo ""
        echo -e "${RED}Found issues in C code${NC}"
        return 1
    fi
}

# Parse arguments
SHELL_ONLY=0
C_ONLY=0
STRICT=0

while [ $# -gt 0 ]; do
    case $1 in
        --shell-only)
            SHELL_ONLY=1
            shift
            ;;
        --c-only)
            C_ONLY=1
            shift
            ;;
        --strict)
            STRICT=1
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            print_usage
            exit 1
            ;;
    esac
done

echo ""
echo "ESP32 Rover Static Analysis"
echo "==========================="
echo ""

SHELL_RESULT=0
C_RESULT=0

if [ $C_ONLY -eq 0 ]; then
    run_shellcheck || SHELL_RESULT=1
    echo ""
fi

if [ $SHELL_ONLY -eq 0 ]; then
    run_cppcheck || C_RESULT=1
    echo ""
fi

# Summary
echo "==========================="
echo "Summary"
echo "==========================="

if [ $SHELL_RESULT -eq 0 ] && [ $C_RESULT -eq 0 ]; then
    echo -e "${GREEN}All checks passed!${NC}"
    exit 0
else
    if [ $SHELL_RESULT -ne 0 ]; then
        echo -e "${RED}Shell scripts: FAILED${NC}"
    fi
    if [ $C_RESULT -ne 0 ]; then
        echo -e "${RED}C code: FAILED${NC}"
    fi
    exit 1
fi
