#!/usr/bin/env bash
#
# NexusDeck: remove the Linux server installed by install-server.sh.
#
# Stops and disables the systemd user unit, removes the unit file, and
# (after confirmation) deletes the venv + state (profiles, settings, cert)
# and undoes the /etc/hosts and cert-trust steps.
#
# Options:
#   --yes    Remove everything without prompting (including venv/state).
#   -h | --help
#
# What is NOT removed: the repo checkout itself, and boot lingering
# (`loginctl disable-linger $USER` if you want that too).

set -euo pipefail

BASE_DIR="$HOME/.local/share/nexusdeck"
VENV_DIR="$BASE_DIR/venv"
DATA_DIR="$BASE_DIR/state"
UNIT_DIR="$HOME/.config/systemd/user"
UNIT_NAME="nexusdeck.service"
UNIT_FILE="$UNIT_DIR/$UNIT_NAME"
LEGACY_UNIT_NAME="nexusdeck-companion.service"   # created by Studio's autostart toggle
LEGACY_UNIT_FILE="$UNIT_DIR/$LEGACY_UNIT_NAME"
CERT_FILE="$DATA_DIR/nexusdeck.crt"
HOSTS_MARKER="# nexusdeck"
ASSUME_YES=0

# ---------- output helpers ----------
if [ -t 1 ]; then
    C_G=$'\033[32m'; C_Y=$'\033[33m'; C_R=$'\033[31m'; C_B=$'\033[36m'; C_0=$'\033[0m'
else
    C_G=''; C_Y=''; C_R=''; C_B=''; C_0=''
fi
info()  { printf '%s[uninstall]%s %s\n' "$C_B" "$C_0" "$*"; }
ok()    { printf '%s  ok %s %s\n'  "$C_G" "$C_0" "$*"; }
warn()  { printf '%s warn%s %s\n'  "$C_Y" "$C_0" "$*"; }
err()   { printf '%s  err%s %s\n'  "$C_R" "$C_0" "$*" >&2; }
die()   { err "$*"; exit 1; }

usage() {
    sed -n '3,14p' "$0" | sed 's/^# \{0,1\}//'
    exit 0
}

# ---------- args ----------
while [ $# -gt 0 ]; do
    case "$1" in
        --yes)      ASSUME_YES=1; shift ;;
        -h|--help)  usage ;;
        *)          die "unknown option: $1 (see --help)" ;;
    esac
done

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        warn "sudo not found; /etc/hosts and CA-store cleanup will be skipped"
    fi
fi
run_root() {
    if [ -n "$SUDO" ]; then "$SUDO" "$@"; else "$@"; fi
}

confirm() {
    [ "$ASSUME_YES" -eq 1 ] && return 0
    local answer
    read -r -p "$1 [y/N] " answer || true
    [ "${answer:-n}" = "y" ] || [ "${answer:-n}" = "Y" ]
}

# ---------- 1. stop and remove the services ----------
remove_service() {
    local name file="$2"
    systemctl --user disable --now "$name" >/dev/null 2>&1 || true
    if [ -f "$file" ]; then
        rm -f "$file"
        ok "removed unit $name"
    fi
    systemctl --user daemon-reload >/dev/null 2>&1 || true
    systemctl --user reset-failed "$name" >/dev/null 2>&1 || true
}

remove_services() {
    remove_service "$UNIT_NAME" "$UNIT_FILE"
    remove_service "$LEGACY_UNIT_NAME" "$LEGACY_UNIT_FILE"
    ok "services stopped and removed"
}

# ---------- 2. venv + state ----------
remove_data() {
    if [ ! -d "$BASE_DIR" ]; then
        ok "nothing to remove at $BASE_DIR"
        return 0
    fi
    echo "  This deletes: venv, state dir (profiles, settings, ESP32 pairing"
    echo "  token state, logs) and the self-signed cert under $BASE_DIR."
    if confirm "Delete $BASE_DIR?"; then
        rm -rf "$BASE_DIR"
        ok "removed $BASE_DIR"
    else
        info "kept $BASE_DIR"
    fi
}

# ---------- 3. /etc/hosts ----------
remove_hosts_entry() {
    if ! grep -qE '^[[:space:]]*127\.0\.0\.1[[:space:]]\{1,\}nexusdeck([[:space:]]|$)' /etc/hosts 2>/dev/null; then
        ok "/etc/hosts has no nexusdeck entry"
        return 0
    fi
    if [ -z "$SUDO" ]; then
        warn "cannot edit /etc/hosts without sudo - remove the 'nexusdeck' line manually"
        return 0
    fi
    run_root sed -i '\|^[[:space:]]*127\.0\.0\.1[[:space:]]\{1,\}nexusdeck|d' /etc/hosts
    ok "removed nexusdeck entry from /etc/hosts"
}

# ---------- 4. cert trust ----------
untrust_cert() {
    if [ "$ASSUME_YES" -ne 1 ] && ! confirm "Also remove the cert from system + browser trust stores?"; then
        info "kept cert trust"
        return 0
    fi
    if [ -f /usr/local/share/ca-certificates/nexusdeck.crt ]; then
        run_root rm -f /usr/local/share/ca-certificates/nexusdeck.crt
        run_root update-ca-certificates --fresh >/dev/null 2>&1 || run_root update-ca-certificates >/dev/null 2>&1 || true
        ok "untrusted (Debian/Ubuntu CA store)"
    fi
    if [ -f /etc/pki/ca-trust/source/anchors/nexusdeck.crt ]; then
        run_root rm -f /etc/pki/ca-trust/source/anchors/nexusdeck.crt
        run_root update-ca-trust >/dev/null 2>&1 || true
        ok "untrusted (Fedora/RHEL CA store)"
    fi
    if [ -f /etc/ca-certificates/trust-source/anchors/nexusdeck.crt ]; then
        run_root rm -f /etc/ca-certificates/trust-source/anchors/nexusdeck.crt
        run_root update-ca-trust >/dev/null 2>&1 || true
        ok "untrusted (Arch CA store)"
    fi
    if command -v certutil >/dev/null 2>&1; then
        if certutil -L -d "sql:$HOME/.pki/nssdb" -n nexusdeck >/dev/null 2>&1; then
            certutil -D -d "sql:$HOME/.pki/nssdb" -n nexusdeck >/dev/null 2>&1 || true
            ok "untrusted in Chromium NSS db"
        fi
    fi
    warn "Firefox: remove 'nexusdeck' manually (Settings > Privacy > Certificates > View)"
}

print_summary() {
    echo
    printf '%s==============================================================%s\n' "$C_G" "$C_0"
    echo "  NexusDeck uninstalled."
    echo "  Kept: the repo checkout, and boot lingering (optional:"
    echo "  sudo loginctl disable-linger $USER)."
    printf '%s==============================================================%s\n' "$C_G" "$C_0"
}

main() {
    remove_services
    remove_data
    remove_hosts_entry
    untrust_cert
    print_summary
}

main
