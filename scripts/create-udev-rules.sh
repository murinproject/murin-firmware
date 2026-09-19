#!/usr/bin/env bash

set -euo pipefail

RULES_FILE="/etc/udev/rules.d/99-murin.rules"
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_RULES_FILE="$SCRIPT_DIR/99-murin.rules"

if [[ "$(id -u)" -ne 0 ]]; then
	printf 'Run this script as root, for example: sudo %s\n' "$0" >&2
	exit 1
fi

if [[ ! -f "$SOURCE_RULES_FILE" ]]; then
	printf 'Rules file not found: %s\n' "$SOURCE_RULES_FILE" >&2
	exit 1
fi

install -D -m 0644 "$SOURCE_RULES_FILE" "$RULES_FILE"
udevadm control --reload-rules
udevadm trigger --subsystem-match=tty

printf 'Installed %s\n' "$RULES_FILE"
printf 'Console: /dev/murin-console\n'
printf 'CDC:     /dev/murin-cdc\n'
