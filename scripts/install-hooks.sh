#!/usr/bin/env bash
#
# install-hooks.sh — install (or remove) an optional git pre-push hook.
#
# Purpose:
#   The hook runs the same checks as CI locally before any push:
#       bash scripts/ci-local.sh --no-lint
#   A failing run aborts the push. It is opt-in: nothing happens until you
#   run this script, and nothing is committed to the repository.
#
# Usage:
#   bash scripts/install-hooks.sh              # install the pre-push hook
#   bash scripts/install-hooks.sh --uninstall  # remove it again
#   bash scripts/install-hooks.sh --help       # usage
#
# Notes:
#   * The hook runs a CLEAN build (ci-local.sh removes build-local-ci unless
#     --keep-build is passed), so expect a few minutes before each push.
#   * Only hooks carrying the marker line below are considered ours: a
#     foreign pre-push hook is never deleted — it is copied to
#     .git/hooks/pre-push.bak and then replaced, so nothing is lost.
#   * --uninstall refuses (non-zero) if the installed hook is not ours.
#
# Exit status: 0 on success, non-zero on refusal or write failure.

set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

MARKER="# lumalarm-managed-hook"
GIT_DIR="$(git rev-parse --git-dir)"
HOOK="$GIT_DIR/hooks/pre-push"
BACKUP="$GIT_DIR/hooks/pre-push.bak"

usage() {
    cat <<EOF
Usage: bash scripts/install-hooks.sh [--uninstall]

Installs a pre-push hook that runs 'bash scripts/ci-local.sh --no-lint'
and aborts the push when it fails.

  (no argument)  install the hook (backs up a foreign hook to pre-push.bak)
  --uninstall    remove the hook, but only if it carries:
                 $MARKER
  --help         show this help and exit
EOF
}

ACTION="install"
if [ $# -gt 0 ]; then
    case "$1" in
        --uninstall) ACTION="uninstall" ;;
        --help|-h)   usage; exit 0 ;;
        *)           echo "install-hooks: unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
fi

hook_is_ours() {
    [ -f "$HOOK" ] && grep -qxF "$MARKER" "$HOOK"
}

write_hook() {
    mkdir -p "$GIT_DIR/hooks"
    cat > "$HOOK" <<'HOOKEOF'
#!/usr/bin/env bash
# lumalarm-managed-hook
#
# Installed by scripts/install-hooks.sh — the real policy lives in the
# repository, not here. Regenerate with:
#     bash scripts/install-hooks.sh
# Remove with:
#     bash scripts/install-hooks.sh --uninstall
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

echo "[pre-push] running local CI: bash scripts/ci-local.sh --no-lint"

if bash scripts/ci-local.sh --no-lint; then
    echo "[pre-push] local CI passed"
    exit 0
fi

echo "[pre-push] local CI FAILED — aborting push." >&2
exit 1
HOOKEOF
    chmod +x "$HOOK"
}

install_hook() {
    if [ -e "$HOOK" ]; then
        if hook_is_ours; then
            echo "[install-hooks] replacing existing Lumalarm pre-push hook"
        else
            # Never destroy someone else's hook: keep a copy first.
            cp -p "$HOOK" "$BACKUP"
            echo "[install-hooks] existing pre-push hook is NOT managed by Lumalarm;"
            echo "[install-hooks] preserved a copy at $BACKUP"
            echo "[install-hooks] merge it back manually if you need both"
        fi
    else
        echo "[install-hooks] no existing pre-push hook"
    fi

    write_hook

    if bash -n "$HOOK"; then
        echo "[install-hooks] installed $HOOK (syntax check OK)"
    else
        echo "[install-hooks] ERROR: installed hook failed 'bash -n'" >&2
        exit 1
    fi
    echo "[install-hooks] it runs 'bash scripts/ci-local.sh --no-lint' and aborts on failure"
}

uninstall_hook() {
    if [ ! -e "$HOOK" ]; then
        echo "[install-hooks] no pre-push hook installed — nothing to do"
        return 0
    fi
    if ! hook_is_ours; then
        echo "[install-hooks] refusing to remove $HOOK: it is not managed by Lumalarm" >&2
        echo "[install-hooks] (no '$MARKER' line) — left untouched" >&2
        return 1
    fi
    rm -f "$HOOK"
    echo "[install-hooks] removed $HOOK"
}

case "$ACTION" in
    install)   install_hook ;;
    uninstall) uninstall_hook ;;
esac
