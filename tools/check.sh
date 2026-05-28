#!/bin/bash
# 手動チェック: cppcheck + pio run を一括実行
#
# Usage:
#   ./tools/check.sh                  # 全部
#   ./tools/check.sh lint             # cppcheck のみ
#   ./tools/check.sh build            # pio build のみ
#   ./tools/check.sh diff             # 変更ファイル (HEAD vs main) のみ cppcheck

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || {
  echo "ERROR: not in a git repository"
  exit 1
}
cd "$REPO_ROOT"

MODE="${1:-all}"

run_lint_full() {
  if ! command -v cppcheck >/dev/null 2>&1; then
    echo "cppcheck not installed (brew install cppcheck)"
    return 1
  fi
  echo "=== cppcheck (full) ==="
  cppcheck \
    --enable=warning,performance,portability \
    --quiet \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    --suppress=unusedFunction \
    --error-exitcode=0 \
    -I firmware/src \
    firmware/src
}

run_lint_diff() {
  if ! command -v cppcheck >/dev/null 2>&1; then
    echo "cppcheck not installed (brew install cppcheck)"
    return 1
  fi
  echo "=== cppcheck (changed vs origin/main) ==="
  files=$(git diff --name-only origin/main...HEAD | grep -E '^firmware/src/.*\.(cpp|h)$' || true)
  if [ -z "$files" ]; then
    echo "(no changed C++ files)"
    return 0
  fi
  echo "$files" | xargs cppcheck \
    --enable=warning,performance,portability \
    --quiet \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    --suppress=unusedFunction \
    --error-exitcode=0
}

run_build() {
  echo "=== pio run -e m5stack-cores3 ==="
  PIO="$HOME/.platformio/penv/bin/pio"
  if [ ! -x "$PIO" ]; then
    echo "pio not found at $PIO"
    return 1
  fi
  (cd firmware && "$PIO" run -e m5stack-cores3)
}

case "$MODE" in
  lint)  run_lint_full ;;
  diff)  run_lint_diff ;;
  build) run_build ;;
  all)
    run_lint_diff
    echo ""
    run_build
    ;;
  *)
    echo "Usage: $0 [lint|diff|build|all]"
    exit 1
    ;;
esac
