#!/bin/bash
# 開発者環境セットアップ: git pre-push hook を有効化
#
# 1 回だけ実行:
#   ./tools/setup-hooks.sh

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || {
  echo "ERROR: not in a git repository"
  exit 1
}
cd "$REPO_ROOT"

if [ ! -d ".githooks" ]; then
  echo "ERROR: .githooks/ directory not found in $REPO_ROOT"
  exit 1
fi

chmod +x .githooks/pre-push 2>/dev/null || true

# このリポジトリだけ hooks path を変更（global には影響しない）
git config core.hooksPath .githooks

echo "✓ git config core.hooksPath = .githooks"
echo ""
echo "次回 git push 時に以下が自動実行されます:"
echo "  - cppcheck on firmware/src/**/*.{cpp,h}"
echo "  - pio run -e m5stack-cores3"
echo ""
echo "スキップしたい場合: git push --no-verify"
echo ""
echo "推奨: brew install cppcheck"
if ! command -v cppcheck >/dev/null 2>&1; then
  echo "  ⚠ cppcheck 未インストール（hook は警告のみで続行）"
fi
