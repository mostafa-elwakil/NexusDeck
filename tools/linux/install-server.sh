#!/usr/bin/env bash
#
# NexusDeck: install the companion server on Linux.
# Run this from a cloned NexusDeck repo checkout:
#     tools/linux/install-server.sh
#
# What it does (idempotent - safe to re-run):
#   1. Checks python3 >= 3.10 and installs system deps (apt-get / dnf / pacman).
#   2. Creates a venv at ~/.local/share/nexusdeck/venv and pip-installs
#      streamdeck-simulator/server/requirements.txt.
#   3. Pre-generates the self-signed cert via the server itself
#      (python server.py --ensure-cert) into
#      ~/.local/share/nexusdeck/state/nexusdeck.crt + .key.
#      The server generates its own cert with cryptography; no openssl needed.
#   4. Adds "127.0.0.1 nexusdeck" to /etc/hosts (marker: "# nexusdeck").
#   5. Trusts the cert system-wide (update-ca-certificates / update-ca-trust)
#      and optionally in the Chrome/Chromium NSS db (certutil).
#   6. Installs a systemd user unit (nexusdeck.service) running the server,
#      then `systemctl --user enable --now nexusdeck`.
#   7. Enables lingering so it runs at boot without login (best effort).
#
# Options:
#   --repo DIR        Repo checkout containing streamdeck-simulator/server
#                     (default: this script's repo, or $NEXUSDECK_REPO).
#   --https-port N    HTTPS port for https://nexusdeck (default 8443;
#                     the server's own default 443 needs root/sysctl).
#   -h | --help       Show this help.
#
# After install: https://nexusdeck:8443/  (Studio) and complete ESP32
# pairing in the web UI. See README-linux.md.

set -euo pipefail

SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_REPO="$(cd "$SCRIPT_PATH/../.." && pwd)"
REPO="${NEXUSDECK_REPO:-$DEFAULT_REPO}"
HTTPS_PORT="${NEXUSDECK_HTTPS_PORT:-8443}"

BASE_DIR="$HOME/.local/share/nexusdeck"
VENV_DIR="$BASE_DIR/venv"
DATA_DIR="$BASE_DIR/state"          # NEXUSDECK_DATA_DIR for the service
UNIT_DIR="$HOME/.config/systemd/user"
UNIT_NAME="nexusdeck.service"
UNIT_FILE="$UNIT_DIR/$UNIT_NAME"
LEGACY_UNIT_NAME="nexusdeck-companion.service"   # created by Studio's autostart toggle
LEGACY_UNIT_FILE="$UNIT_DIR/$LEGACY_UNIT_NAME"
CERT_FILE="$DATA_DIR/nexusdeck.crt"
KEY_FILE="$DATA_DIR/nexusdeck.key"
HOSTS_MARKER="# nexusdeck"

# ---------- output helpers (repo tools tone: plain + colored) ----------
if [ -t 1 ]; then
    C_G=$'\033[32m'; C_Y=$'\033[33m'; C_R=$'\033[31m'; C_B=$'\033[36m'; C_0=$'\033[0m'
else
    C_G=''; C_Y=''; C_R=''; C_B=''; C_0=''
fi
info()  { printf '%s[install]%s %s\n' "$C_B" "$C_0" "$*"; }
ok()    { printf '%s  ok %s %s\n'  "$C_G" "$C_0" "$*"; }
warn()  { printf '%s warn%s %s\n'  "$C_Y" "$C_0" "$*"; }
err()   { printf '%s  err%s %s\n'  "$C_R" "$C_0" "$*" >&2; }
die()   { err "$*"; exit 1; }

usage() {
    sed -n '2,30p' "$SCRIPT_PATH/$(basename "${BASH_SOURCE[0]}")" | sed 's/^# \{0,1\}//'
    exit 0
}

# ---------- args ----------
while [ $# -gt 0 ]; do
    case "$1" in
        --repo)       REPO="${2:?--repo needs a directory}"; shift 2 ;;
        --https-port) HTTPS_PORT="${2:?--https-port needs a number}"; shift 2 ;;
        -h|--help)    usage ;;
        *)            die "unknown option: $1 (see --help)" ;;
    esac
done
# Computed after parsing so --repo / NEXUSDECK_REPO take effect.
SERVER_DIR="$REPO/streamdeck-simulator/server"

# ---------- sudo helper ----------
SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        warn "sudo not found; system steps (hosts, cert trust, lingering) will be skipped"
    fi
fi
run_root() {
    if [ -n "$SUDO" ]; then "$SUDO" "$@"; else "$@"; fi
}

# ---------- 0. locate the server sources ----------
check_repo() {
    [ -f "$SERVER_DIR/server.py" ] || die "server.py not found at $SERVER_DIR/server.py - run from a cloned repo checkout or pass --repo DIR"
    [ -f "$SERVER_DIR/requirements.txt" ] || die "requirements.txt not found in $SERVER_DIR"
}

# ---------- 1. python >= 3.10 + system deps ----------
find_python() {
    local candidate
    for candidate in python3 python; do
        if command -v "$candidate" >/dev/null 2>&1 \
           && "$candidate" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)' >/dev/null 2>&1; then
            printf '%s' "$candidate"
            return 0
        fi
    done
    return 1
}

detect_pkg() {
    local manager
    for manager in apt-get dnf pacman; do
        if command -v "$manager" >/dev/null 2>&1; then
            printf '%s' "$manager"
            return 0
        fi
    done
    return 1
}

install_system_deps() {
    local pkg
    pkg="$(detect_pkg)" || {
        warn "unsupported distro: install python3 (>= 3.10), pip and venv manually, then re-run"
        return 1
    }
    info "installing system packages via $pkg ..."
    case "$pkg" in
        apt-get)
            run_root apt-get update -qq || true
            run_root apt-get install -y python3 python3-venv python3-pip ;;
        dnf)
            run_root dnf install -y python3 python3-pip ;;
        pacman)
            run_root pacman -Sy --noconfirm python python-pip ;;
    esac
}

check_python() {
    local py
    if py="$(find_python)"; then
        PYTHON="$py"
        ok "python3 >= 3.10 found: $PYTHON ($("$PYTHON" -c 'import sys; print(".".join(map(str, sys.version_info[:2])))'))"
        return 0
    fi
    warn "python3 >= 3.10 not found - trying package manager"
    install_system_deps || return 1
    if py="$(find_python)"; then
        PYTHON="$py"
        ok "python3 installed: $PYTHON"
        return 0
    fi
    die "no python3 >= 3.10 available; install it manually and re-run"
}

# ---------- 2. venv + requirements ----------
setup_venv() {
    if [ -x "$VENV_DIR/bin/python" ]; then
        ok "venv already present: $VENV_DIR"
    else
        info "creating venv at $VENV_DIR ..."
        mkdir -p "$BASE_DIR"
        "$PYTHON" -m venv "$VENV_DIR" || die "failed to create venv (Debian/Ubuntu: apt-get install python3-venv)"
        ok "venv created"
    fi
    info "installing Python dependencies (flask, psutil, cryptography, Pillow, ...) ..."
    "$VENV_DIR/bin/python" -m pip install --upgrade pip >/dev/null 2>&1 || true
    "$VENV_DIR/bin/python" -m pip install -r "$SERVER_DIR/requirements.txt" \
        || die "pip install failed (very old distros may need gcc / libffi-devel for Pillow & cryptography; modern wheels cover x86_64 + aarch64)"
    ok "dependencies installed"
}

# ---------- 3. self-signed cert (generated by the server itself) ----------
# server.py generates nexusdeck.crt / nexusdeck.key inside NEXUSDECK_DATA_DIR
# with cryptography (SAN: nexusdeck, localhost, 127.0.0.1 + LAN IPs, 10 years).
# `--ensure-cert` only writes the files and exits - no server is started.
ensure_cert() {
    info "generating self-signed cert (server.py --ensure-cert) ..."
    ( cd "$SERVER_DIR" && NEXUSDECK_DATA_DIR="$DATA_DIR" "$VENV_DIR/bin/python" server.py --ensure-cert )
    [ -f "$CERT_FILE" ] && [ -f "$KEY_FILE" ] \
        || die "cert generation failed; start the server once and re-run"
    ok "cert ready: $CERT_FILE"
}

# ---------- 4. /etc/hosts ----------
add_hosts_entry() {
    if grep -qE '^[[:space:]]*127\.0\.0\.1[[:space:]]\{1,\}nexusdeck([[:space:]]|$)' /etc/hosts 2>/dev/null; then
        ok "/etc/hosts already maps nexusdeck -> 127.0.0.1"
        return 0
    fi
    if [ -z "$SUDO" ]; then
        warn "cannot edit /etc/hosts without sudo - add manually: 127.0.0.1 nexusdeck"
        return 0
    fi
    printf '127.0.0.1\tnexusdeck\t%s\n' "$HOSTS_MARKER" | run_root tee -a /etc/hosts >/dev/null
    ok "added '127.0.0.1 nexusdeck' to /etc/hosts"
}

# ---------- 5. trust the cert ----------
trust_cert() {
    info "trusting cert system-wide ..."
    if command -v update-ca-certificates >/dev/null 2>&1 && [ -d /usr/local/share/ca-certificates ]; then
        if run_root cmp -s "$CERT_FILE" /usr/local/share/ca-certificates/nexusdeck.crt 2>/dev/null; then
            ok "already trusted (Debian/Ubuntu CA store)"
            return 0
        fi
        run_root cp "$CERT_FILE" /usr/local/share/ca-certificates/nexusdeck.crt
        run_root update-ca-certificates >/dev/null
        ok "trusted in Debian/Ubuntu CA store"
    elif [ -d /etc/pki/ca-trust/source/anchors ]; then
        if run_root cmp -s "$CERT_FILE" /etc/pki/ca-trust/source/anchors/nexusdeck.crt 2>/dev/null; then
            ok "already trusted (Fedora/RHEL CA store)"
            return 0
        fi
        run_root cp "$CERT_FILE" /etc/pki/ca-trust/source/anchors/nexusdeck.crt
        run_root update-ca-trust >/dev/null
        ok "trusted in Fedora/RHEL CA store"
    elif [ -d /etc/ca-certificates/trust-source/anchors ]; then
        if run_root cmp -s "$CERT_FILE" /etc/ca-certificates/trust-source/anchors/nexusdeck.crt 2>/dev/null; then
            ok "already trusted (Arch CA store)"
            return 0
        fi
        run_root cp "$CERT_FILE" /etc/ca-certificates/trust-source/anchors/nexusdeck.crt
        run_root update-ca-trust >/dev/null
        ok "trusted in Arch CA store"
    else
        warn "no known CA store found - Chrome may still show a warning (Advanced > Proceed)"
    fi

    # Chrome/Chromium on Linux reads the NSS db, not /etc/ssl (best effort).
    if command -v certutil >/dev/null 2>&1; then
        local nssdb="$HOME/.pki/nssdb"
        mkdir -p "$nssdb"
        certutil -N -d "sql:$nssdb" --empty-password >/dev/null 2>&1 || true
        if certutil -A -d "sql:$nssdb" -n nexusdeck -t 'C,,' -i "$CERT_FILE" >/dev/null 2>&1; then
            ok "trusted in Chromium NSS db ($nssdb)"
        else
            warn "could not update NSS db ($nssdb)"
        fi
    else
        warn "certutil (libnss3-tools / nss-tools) not installed - Chrome/Chromium may show a cert warning;"
        warn "  install it and re-run, or accept the warning once (Advanced > Proceed)."
    fi
    warn "Firefox uses its own profile store: Settings > Privacy > Certificates > View > Import '$CERT_FILE'"
}

# ---------- 6. systemd user unit ----------
# Matches server.py's dev invocation (python server.py, cwd = server dir).
# Env vars map to real code: NEXUSDECK_DATA_DIR (state/cert dir override) and
# NEXUSDECK_HTTPS_PORT (HTTPS listener port; user services cannot bind 443).
remove_legacy_unit() {
    # Studio's "run in background" toggle writes nexusdeck-companion.service
    # (SYSTEMD_UNIT_NAME in server.py). Two units -> duplicate server on
    # port 8765. The installer-managed unit replaces it.
    if [ -f "$LEGACY_UNIT_FILE" ]; then
        systemctl --user disable --now "$LEGACY_UNIT_NAME" >/dev/null 2>&1 || true
        rm -f "$LEGACY_UNIT_FILE"
        systemctl --user daemon-reload >/dev/null 2>&1 || true
        warn "removed legacy unit $LEGACY_UNIT_NAME (Studio autostart toggle)"
    fi
}

write_unit() {
    info "writing systemd user unit: $UNIT_FILE"
    mkdir -p "$UNIT_DIR"
    cat > "$UNIT_FILE" <<EOF
[Unit]
Description=NexusDeck Companion Server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
WorkingDirectory=$SERVER_DIR
Environment="NEXUSDECK_DATA_DIR=$DATA_DIR"
Environment="NEXUSDECK_HTTPS_PORT=$HTTPS_PORT"
ExecStart="$VENV_DIR/bin/python" "server.py"
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
EOF
}

start_service() {
    systemctl --user daemon-reload
    if systemctl --user enable --now "$UNIT_NAME"; then
        ok "service enabled and started (systemctl --user status $UNIT_NAME)"
    else
        warn "could not start the user service (no systemd user session?)."
        warn "  Log out and back in (or reboot), then: systemctl --user start $UNIT_NAME"
    fi
}

enable_lingering() {
    if [ -z "$SUDO" ]; then
        warn "skipped lingering (needs sudo) - the server starts at login only"
        return 0
    fi
    if run_root loginctl enable-linger "$USER" 2>/dev/null; then
        ok "lingering enabled - server runs at boot without login"
    else
        warn "could not enable lingering - the server starts at login only"
    fi
}

# ---------- summary ----------
print_summary() {
    echo
    printf '%s==============================================================%s\n' "$C_G" "$C_0"
    printf '%s NexusDeck installed%s\n' "$C_G" "$C_0"
    printf '%s==============================================================%s\n' "$C_G" "$C_0"
    echo "  Studio UI     : https://nexusdeck:$HTTPS_PORT/   (https://nexusdeck/ when the HTTPS port is 443)"
    echo "                  fallback: http://localhost:8765/"
    echo "  State dir     : $DATA_DIR   (NEXUSDECK_DATA_DIR)"
    echo "  Cert          : $CERT_FILE"
    echo "  Service       : systemctl --user status $UNIT_NAME"
    echo "  Logs          : journalctl --user -u $UNIT_NAME -f"
    echo
    echo "  Next: open the Studio UI and complete ESP32 pairing in the"
    echo "  web UI (the device keeps using http://<this-ip>:8765/)."
    echo
    echo "  NOTE: the repo checkout must stay at $REPO -"
    echo "  the service runs the server from it. Re-run this script"
    echo "  (or pass --repo) after moving or updating the repo."
    printf '%s==============================================================%s\n' "$C_G" "$C_0"
}

main() {
    check_repo
    check_python
    setup_venv
    ensure_cert
    add_hosts_entry
    trust_cert
    remove_legacy_unit
    write_unit
    if [ "$HTTPS_PORT" -lt 1024 ] 2>/dev/null; then
        warn "HTTPS port $HTTPS_PORT < 1024: a user service cannot bind it;"
        warn "  the server falls back to 8443 automatically, or allow it via:"
        warn "  sudo sysctl -w net.ipv4.ip_unprivileged_port_start=0"
    fi
    start_service
    enable_lingering
    print_summary
}

main
