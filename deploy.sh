#!/bin/bash
# TizenClaw Sample LLM Plugin Build, Deploy & Run Script
# Automates: gbs build → sdb push → rpm install → unified-backend (app register)
#
# Usage:
#   ./deploy.sh                    # Full pipeline (build + deploy)
#   ./deploy.sh -s                 # Skip build, deploy only
#   ./deploy.sh -n                 # Use --noinit for faster rebuild
#   ./deploy.sh --dry-run          # Print commands without executing
#   ./deploy.sh -d <serial>        # Target a specific sdb device
#
# See ./deploy.sh --help for all options.

set -euo pipefail

# ─────────────────────────────────────────────
# Constants
# ─────────────────────────────────────────────
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
PKG_NAME="tizenclaw-llm-plugin-sample"
APP_ID="org.tizen.tizenclaw-llm-plugin-sample"
GBS_BUILD_LOG="/tmp/${PKG_NAME}_gbs_build_output.log"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ─────────────────────────────────────────────
# Defaults
# ─────────────────────────────────────────────
ARCH=""
ARCH_EXPLICIT=false
NOINIT=false
SKIP_BUILD=false
DRY_RUN=false
DEVICE_SERIAL=""

# ─────────────────────────────────────────────
# Logging helpers
# ─────────────────────────────────────────────
log()    { echo -e "${CYAN}[DEPLOY]${NC} $*"; }
ok()     { echo -e "${GREEN}[  OK  ]${NC} $*"; }
warn()   { echo -e "${YELLOW}[ WARN ]${NC} $*"; }
fail()   { echo -e "${RED}[ FAIL ]${NC} $*"; exit 1; }
header() { echo -e "\n${BOLD}══════════════════════════════════════════${NC}"; echo -e "${BOLD}  $*${NC}"; echo -e "${BOLD}══════════════════════════════════════════${NC}"; }

# ─────────────────────────────────────────────
# sdb wrapper (supports -s <serial>)
# ─────────────────────────────────────────────
sdb_cmd() {
  if [ -n "${DEVICE_SERIAL}" ]; then
    sdb -s "${DEVICE_SERIAL}" "$@"
  else
    sdb "$@"
  fi
}

sdb_shell() {
  sdb_cmd shell "$@"
}

# ─────────────────────────────────────────────
# Auto-detect device architecture via sdb
# ─────────────────────────────────────────────
detect_arch() {
  # If user explicitly specified arch via -a, skip auto-detection
  if [ "${ARCH_EXPLICIT}" = true ]; then
    log "Using explicit architecture: ${ARCH}"
    return 0
  fi

  log "Auto-detecting device architecture via sdb..."

  local sdb_cap_cmd=(sdb)
  if [ -n "${DEVICE_SERIAL}" ]; then
    sdb_cap_cmd=(sdb -s "${DEVICE_SERIAL}")
  fi

  local cpu_arch
  cpu_arch=$("${sdb_cap_cmd[@]}" capability 2>/dev/null | grep '^cpu_arch:' | cut -d':' -f2 || true)

  if [ -z "${cpu_arch}" ]; then
    warn "Could not detect device architecture. Falling back to x86_64"
    ARCH="x86_64"
    return 0
  fi

  # Map sdb cpu_arch to GBS-compatible architecture name
  case "${cpu_arch}" in
    armv7)   ARCH="armv7l" ;;
    *)       ARCH="${cpu_arch}" ;;
  esac

  ok "Detected device architecture: ${ARCH} (cpu_arch: ${cpu_arch})"
}

# ─────────────────────────────────────────────
# Dry-run wrapper
# ─────────────────────────────────────────────
run() {
  if [ "${DRY_RUN}" = true ]; then
    echo -e "  ${YELLOW}[DRY-RUN]${NC} $*"
    return 0
  fi
  "$@"
}

# ─────────────────────────────────────────────
# Usage
# ─────────────────────────────────────────────
usage() {
  cat <<EOF
${BOLD}TizenClaw Sample LLM Plugin Build & Deploy${NC}

${CYAN}Usage:${NC}
  $(basename "$0") [options]

${CYAN}Options:${NC}
  -a, --arch <arch>     Build architecture (default: auto-detect via sdb)
  -n, --noinit          Skip build-env init (faster rebuild)
  -s, --skip-build      Skip GBS build, deploy existing RPM
  -d, --device <serial> Target a specific sdb device
      --dry-run         Print commands without executing
  -h, --help            Show this help

${CYAN}Examples:${NC}
  $(basename "$0")                     # Full build + deploy
  $(basename "$0") -n                  # Quick rebuild + deploy
  $(basename "$0") -s                  # Deploy existing RPM
  $(basename "$0") --dry-run           # Preview all steps
  $(basename "$0") -a aarch64          # Build for ARM64 target
EOF
  exit 0
}

# ─────────────────────────────────────────────
# Argument parsing
# ─────────────────────────────────────────────
parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      -a|--arch)       ARCH="$2"; ARCH_EXPLICIT=true; shift 2 ;;
      -n|--noinit)     NOINIT=true; shift ;;
      -s|--skip-build) SKIP_BUILD=true; shift ;;
      -d|--device)     DEVICE_SERIAL="$2"; shift 2 ;;
      --dry-run)       DRY_RUN=true; shift ;;
      -h|--help)       usage ;;
      *)               fail "Unknown option: $1 (use --help)" ;;
    esac
  done
}

# ─────────────────────────────────────────────
# Step 0: Pre-flight checks
# ─────────────────────────────────────────────
check_prerequisites() {
  header "Pre-flight Checks"

  if [ "${SKIP_BUILD}" = false ]; then
    if ! command -v gbs &>/dev/null; then
      if [ "${DRY_RUN}" = true ]; then
        warn "gbs not found (ignored in dry-run)"
      else
        fail "gbs not found. Install Tizen GBS first."
      fi
    else
      ok "gbs found"
    fi
  fi

  if ! command -v sdb &>/dev/null; then
    if [ "${DRY_RUN}" = true ]; then
      warn "sdb not found (ignored in dry-run)"
    else
      fail "sdb not found. Install Tizen sdb first."
    fi
  else
    ok "sdb found"
  fi

  log "Architecture : ${ARCH}"
  log "Project dir  : ${PROJECT_DIR}"
  log "Skip build   : ${SKIP_BUILD}"
  log "No-init      : ${NOINIT}"
  log "Dry-run      : ${DRY_RUN}"
  if [ -n "${DEVICE_SERIAL}" ]; then
    log "Device       : ${DEVICE_SERIAL}"
  fi
}

# ─────────────────────────────────────────────
# Step 1: GBS Build
# ─────────────────────────────────────────────
do_build() {
  if [ "${SKIP_BUILD}" = true ]; then
    log "Skipping build (--skip-build)"
    return 0
  fi

  header "Step 1/3: GBS Build"

  local gbs_args=("-A" "${ARCH}" "--include-all")
  if [ "${NOINIT}" = true ]; then
    gbs_args+=("--noinit")
    log "Using --noinit (skipping build-env initialization)"
  fi

  log "Running: gbs build ${gbs_args[*]}"
  cd "${PROJECT_DIR}"

  if [ "${DRY_RUN}" = true ]; then
    echo -e "  ${YELLOW}[DRY-RUN]${NC} gbs build ${gbs_args[*]}"
    ok "GBS build succeeded"
    return 0
  fi

  if gbs build "${gbs_args[@]}" 2>&1 | tee "${GBS_BUILD_LOG}"; then
    ok "GBS build succeeded"
  else
    fail "GBS build failed. Check the build log: ${GBS_BUILD_LOG}"
  fi
  
  RPMS_DIR=$(grep -A1 'generated RPM packages can be found from local repo:' "${GBS_BUILD_LOG}" \
    | tail -1 | sed 's/^[[:space:]]*//')
}

# ─────────────────────────────────────────────
# Step 2: Find the built RPM
# ─────────────────────────────────────────────
RPM_FILES=()
RPMS_DIR=""

find_rpm() {
  header "Step 2/3: Locating RPM"

  if [ -z "${RPMS_DIR}" ]; then
    if [ -f "${GBS_BUILD_LOG}" ]; then
      RPMS_DIR=$(grep -A1 'generated RPM packages can be found from local repo:' "${GBS_BUILD_LOG}" \
        | tail -1 | sed 's/^[[:space:]]*//')
    fi
    if [ -z "${RPMS_DIR}" ]; then
      local gbs_root="${HOME}/GBS-ROOT"
      RPMS_DIR=$(find "${gbs_root}" -type d -path "*/${ARCH}/RPMS" 2>/dev/null | head -1 || true)
    fi
  fi

  if [ "${DRY_RUN}" = true ]; then
    if [ -z "${RPMS_DIR}" ]; then
      RPMS_DIR="${HOME}/GBS-ROOT/local/repos/tizen/${ARCH}/RPMS"
    fi
    RPM_FILES=("${RPMS_DIR}/${PKG_NAME}-1.0.0-1.${ARCH}.rpm")
    log "[DRY-RUN] Assuming RPMs: ${RPM_FILES[*]}"
    return 0
  fi

  if [ -z "${RPMS_DIR}" ] || [ ! -d "${RPMS_DIR}" ]; then
    fail "RPMS directory not found: ${RPMS_DIR:-unknown}\n       Have you run a GBS build first?"
  fi

  log "Searching in: ${RPMS_DIR}"

  mapfile -t RPM_FILES < <(find "${RPMS_DIR}" -maxdepth 1 \
    -name "${PKG_NAME}*.rpm" ! -name "*-debuginfo-*" ! -name "*-debugsource-*" 2>/dev/null | sort)

  if [ ${#RPM_FILES[@]} -eq 0 ]; then
    fail "No ${PKG_NAME} RPMs found in ${RPMS_DIR}/\n       Run a build first or remove --skip-build"
  fi

  for rpm in "${RPM_FILES[@]}"; do
    local rpm_size=$(du -h "${rpm}" | cut -f1)
    ok "Found: $(basename "${rpm}") (${rpm_size})"
  done
}

# ─────────────────────────────────────────────
# Step 3: Deploy via sdb
# ─────────────────────────────────────────────
do_deploy() {
  header "Step 3/3: Deploy to Device"

  log "Checking device connectivity..."
  if [ "${DRY_RUN}" = false ]; then
    local device_list
    device_list=$(sdb devices 2>/dev/null | tail -n +2 | grep -v "^$" || true)

    if [ -z "${device_list}" ]; then
      fail "No sdb devices connected.\n       Start a Tizen Emulator or connect a device."
    fi
    ok "Device connected"
  fi

  log "Acquiring root access..."
  run sdb_cmd root on
  
  log "Remounting root filesystem as read-write..."
  run sdb_shell mount -o remount,rw /

  for rpm in "${RPM_FILES[@]}"; do
    local rpm_basename=$(basename "${rpm}")
    log "Pushing ${rpm_basename} to device:/tmp/"
    run sdb_cmd push "${rpm}" /tmp/

    log "Installing ${rpm_basename}..."
    run sdb_shell rpm -Uvh --force "/tmp/${rpm_basename}"

    log "Cleaning up /tmp/${rpm_basename}..."
    run sdb_shell rm -f "/tmp/${rpm_basename}"

    log "Preloading registry for ${APP_ID}..."
    run sdb_shell unified-backend --preload -y "${APP_ID}"
    ok "Plugin registered to registry"
  done

  log "Restarting tizenclaw daemon..."
  run sdb_shell systemctl daemon-reload

  # ─── Shopping POC: Merge configs into TizenClaw ───
  local PLUGIN_RES="/usr/apps/org.tizen.tizenclaw-llm-plugin-sample/res"
  local TIZENCLAW_CFG="/opt/usr/share/tizenclaw/config"

  log "Merging shopping MCP servers into TizenClaw config..."
  run sdb_shell "python3 -c \"
import json, os
cfg_path = '${TIZENCLAW_CFG}/mcp_servers.json'
plugin_path = '${PLUGIN_RES}/mcp_servers_shopping.json'
if os.path.exists(cfg_path) and os.path.exists(plugin_path):
    with open(cfg_path) as f: base = json.load(f)
    with open(plugin_path) as f: shopping = json.load(f)
    base.setdefault('mcpServers', {}).update(shopping.get('mcpServers', {}))
    with open(cfg_path, 'w') as f: json.dump(base, f, indent=2)
    print('Merged MCP servers OK')
else:
    print('Skipping MCP merge (files not found)')
\""

  log "Merging shopping agent role into TizenClaw config..."
  run sdb_shell "python3 -c \"
import json, os
cfg_path = '${TIZENCLAW_CFG}/agent_roles.json'
plugin_path = '${PLUGIN_RES}/agent_role_shopping.json'
if os.path.exists(cfg_path) and os.path.exists(plugin_path):
    with open(cfg_path) as f: base = json.load(f)
    with open(plugin_path) as f: shopping = json.load(f)
    existing = {a['name'] for a in base.get('agents', [])}
    for agent in shopping.get('agents', []):
        if agent['name'] not in existing:
            base['agents'].append(agent)
    with open(cfg_path, 'w') as f: json.dump(base, f, indent=2)
    print('Merged agent roles OK')
else:
    print('Skipping agent role merge (files not found)')
\""

  log "Merging shopping triggers into TizenClaw config..."
  run sdb_shell "python3 -c \"
import json, os
cfg_path = '${TIZENCLAW_CFG}/autonomous_trigger.json'
plugin_path = '${PLUGIN_RES}/autonomous_trigger_shopping.json'
if os.path.exists(cfg_path) and os.path.exists(plugin_path):
    with open(cfg_path) as f: base = json.load(f)
    with open(plugin_path) as f: shopping = json.load(f)
    existing = {r['name'] for r in base.get('trigger_rules', [])}
    for rule in shopping.get('trigger_rules', []):
        if rule['name'] not in existing:
            base['trigger_rules'].append(rule)
    with open(cfg_path, 'w') as f: json.dump(base, f, indent=2)
    print('Merged triggers OK')
else:
    print('Skipping trigger merge (files not found)')
\""

  run sdb_shell systemctl restart tizenclaw
  ok "Daemon restarted"
}

# ─────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────
show_summary() {
  echo ""
  header "Deploy Complete!"
  ok "${PKG_NAME} has been deployed successfully."
  echo ""
  log "The TizenClaw daemon will load the .so plugin."
  log "To verify it loaded successfully, use the following command:"
  log "  sdb shell dlogutil TIZENCLAW"
  echo ""
}

# ─────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────
main() {
  parse_args "$@"
  detect_arch
  check_prerequisites
  do_build
  find_rpm
  do_deploy
  show_summary
}

main "$@"
