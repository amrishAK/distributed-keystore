# Shared helpers for validation-scripts/*.sh. Must be sourced, not executed directly.

project_root() {
  cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd
}

log() {
  echo "[validate] $*" >&2
}

require_command() {
  local cmd="$1" hint="$2"
  command -v "${cmd}" >/dev/null 2>&1 || { log "'${cmd}' not found. ${hint}"; exit 1; }
}

is_wsl() {
  grep -qi microsoft /proc/version 2>/dev/null
}
