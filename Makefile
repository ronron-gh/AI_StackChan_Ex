# AI_StackChan_Ex 開発タスク
#
# 主要ワークフロー:
#   make upload   # check + build + upload （推奨）
#   make build    # コンパイルのみ
#   make check    # 静的解析（変更ファイル）
#   make monitor  # シリアルモニター
#
# 環境変数:
#   ENV=m5stack-cores3          ターゲット env（platformio.ini の [env:...]）
#   PORT=/dev/cu.usbmodem101    書き込み・モニター先

ENV  ?= m5stack-cores3
PORT ?= $(shell ls /dev/cu.usbmodem* 2>/dev/null | head -1)
PIO  := $(HOME)/.platformio/penv/bin/pio

# .PHONY: 実ファイルではなくコマンド
.PHONY: help build check lint upload flash monitor clean envs setup-hooks all

# デフォルトターゲット
help:
	@printf "AI_StackChan_Ex Makefile\n\n"
	@printf "Workflow:\n"
	@printf "  make build      - compile (pio run -e $(ENV))\n"
	@printf "  make check      - cppcheck on changed C++ files\n"
	@printf "  make lint       - cppcheck on all firmware/src\n"
	@printf "  make upload     - check + build + upload (recommended)\n"
	@printf "  make flash      - upload only (skip checks, for emergencies)\n"
	@printf "  make monitor    - serial monitor\n"
	@printf "  make clean      - clean build artifacts\n"
	@printf "  make envs       - list available platformio envs\n"
	@printf "  make setup-hooks - one-time: enable git pre-push hook\n"
	@printf "\nEnvironment overrides:\n"
	@printf "  ENV=m5stack-cores3       (current: $(ENV))\n"
	@printf "  PORT=/dev/cu.usbmodem101 (current: $(PORT))\n"
	@printf "\nExamples:\n"
	@printf "  make upload                              # CoreS3 デフォルト\n"
	@printf "  make ENV=m5stack-core2 upload            # Core2 にビルド&書き込み\n"
	@printf "  make ENV=m5stack-cores3-llm upload       # ModuleLLM 版\n"

# 静的解析（変更ファイルのみ）
check:
	@./tools/check.sh diff

# 静的解析（全体）
lint:
	@./tools/check.sh lint

# ビルドのみ
build:
	@cd firmware && $(PIO) run -e $(ENV)

# 書き込み（check + build + upload）
# embedded では「書き込み前」が一番効く品質ゲート
upload: check build
	@$(MAKE) flash

# 書き込みだけ（check と build をスキップする緊急用）
flash:
	@if [ -z "$(PORT)" ]; then echo "ERROR: PORT not set and no /dev/cu.usbmodem* found"; exit 1; fi
	@echo "→ killing any cat process holding $(PORT)"
	@pkill -9 -f "cat /dev/cu.usbmodem" 2>/dev/null || true
	@sleep 1
	@cd firmware && $(PIO) run -e $(ENV) -t upload --upload-port $(PORT)

# シリアルモニター
monitor:
	@if [ -z "$(PORT)" ]; then echo "ERROR: PORT not set and no /dev/cu.usbmodem* found"; exit 1; fi
	@cd firmware && $(PIO) device monitor -e $(ENV) -p $(PORT) -b 115200

# クリーン
clean:
	@cd firmware && $(PIO) run -e $(ENV) -t clean

# 利用可能 env リスト
envs:
	@grep "^\[env:" firmware/platformio.ini | sed 's/\[env://;s/\]//' | sort

# git pre-push hook の有効化（1 回だけ実行）
setup-hooks:
	@./tools/setup-hooks.sh

# `make all` = `make upload`（embedded の本筋）
all: upload
