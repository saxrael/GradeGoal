#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

FILES=$(find src tests -type f \( -name "*.c" -o -name "*.h" \) 2>/dev/null || true)

if [ -z "$FILES" ]; then
    echo "No C/H files found to scan."
    exit 0
fi

VIOLATIONS=$(grep -En '(//|/\*|\*/)' $FILES || true)

if [ -n "$VIOLATIONS" ]; then
    echo "Hard Zero Comments Rule VIOLATIONS FOUND:" >&2
    echo "$VIOLATIONS" >&2
    exit 1
fi

echo "Hard Zero Comments Audit PASSED (100% compliant across src/ and tests/)."
exit 0
