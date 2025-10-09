#!/usr/bin/env bash
set -euo pipefail

TROUBLE_URL="docs/TROUBLESHOOTING.md"
trap 'echo "❌ Bootstrap failed. Review ${TROUBLE_URL} for fixes." >&2' ERR

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "=== Orpheus SDK Bootstrap ==="

version_ge() {
  [ "$(printf '%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]
}

extract_version() {
  local raw="$1"
  echo "$raw" | grep -oE '[0-9]+(\.[0-9]+)+' | head -n1
}

ensure_tool() {
  local cmd="$1"
  local requirement="$2"
  local hint="$3"

  if ! command -v "$cmd" >/dev/null 2>&1; then
    echo "❌ $cmd not found. $hint" >&2
    exit 1
  fi

  local raw_version
  raw_version=$("$cmd" --version 2>&1 | head -n1)
  local version
  version=$(extract_version "$raw_version")

  if [ -z "$version" ]; then
    echo "❌ Unable to determine $cmd version. $hint" >&2
    exit 1
  fi

  if ! version_ge "$version" "$requirement"; then
    echo "❌ $cmd $version detected, but >= $requirement is required. $hint" >&2
    exit 1
  fi

  echo "✓ $cmd $version"
}

echo "→ Checking required toolchain versions…"
ensure_tool node "18.0.0" "Install Node.js 18 or newer from https://nodejs.org/."
ensure_tool cmake "3.20.0" "Install CMake 3.20+ (https://cmake.org/download/)."

if ! command -v pnpm >/dev/null 2>&1; then
  if command -v npm >/dev/null 2>&1; then
    echo "→ Installing PNPM 8 (requires npm permissions)…"
    npm install -g pnpm@8
  else
    echo "❌ PNPM is missing and npm is unavailable. Install PNPM 8+ from https://pnpm.io/installation." >&2
    exit 1
  fi
fi
ensure_tool pnpm "8.0.0" "Install PNPM 8+ from https://pnpm.io/installation."

HAS_JS_WORKSPACE=false
if [ -f "package.json" ] && [ -f "pnpm-workspace.yaml" ]; then
  HAS_JS_WORKSPACE=true
fi

if $HAS_JS_WORKSPACE; then
  echo "→ Installing dependencies (pnpm install)…"
  if ! pnpm install --frozen-lockfile; then
    echo "→ Falling back to flexible install…"
    pnpm install
  fi

  echo "→ Building packages (best effort)…"
  pnpm -r --filter @orpheus/* run build --if-present || true
else
  echo "→ No JS workspace detected; skipping pnpm install."
fi

echo "→ Configuring local git hooks…"
git config core.hooksPath .githooks
mkdir -p .githooks
if [ ! -f .githooks/pre-commit ]; then
  cat > .githooks/pre-commit <<'HOOK'
#!/usr/bin/env bash
set -euo pipefail
ALLOWLIST_REGEX='^(docs/images/.*\.(png|jpg|jpeg|svg)$|packages/contract/fixtures/golden-audio/.*\.wav$)'
mapfile -t FILES < <(git diff --cached --name-only --diff-filter=ACMR)
check_ext() {
  case "${1,,}" in
    *.wasm|*.node|*.so|*.dylib|*.dll|*.exe|*.a|*.lib|*.o|*.obj|*.bin|*.pdf|*.zip|*.7z|*.tar|*.gz|*.rar|*.mp3|*.wav|*.flac|*.png|*.jpg|*.jpeg|*.gif|*.svg)
      return 0 ;;
    *) return 1 ;;
  esac
}
BLOCKED=()
for f in "${FILES[@]}"; do
  [ -f "$f" ] || continue
  [[ "$f" =~ $ALLOWLIST_REGEX ]] && continue
  if check_ext "$f"; then
    BLOCKED+=("$f (extension)")
    continue
  fi
  if command -v file >/dev/null 2>&1; then
    MIME="$(file -b --mime "$f" || true)"
    if [[ "$MIME" == *"charset=binary"* || "$MIME" == *"application/octet-stream"* ]]; then
      BLOCKED+=("$f ($MIME)")
    fi
  fi
done
if [ "${#BLOCKED[@]}" -gt 0 ]; then
  echo "❌ Commit blocked: binary files are not permitted."
  printf '   Offending files:\n'
  for b in "${BLOCKED[@]}"; do
    printf '     - %s\n' "$b"
  done
  exit 1
fi
HOOK
  chmod +x .githooks/pre-commit
fi

env_template="$REPO_ROOT/.env.example"
if [ ! -f "$env_template" ]; then
  cat > "$env_template" <<'ENV'
# Orpheus SDK — environment template
ORPHEUS_DRIVER=auto
ORPHEUS_SERVICE_HOST=127.0.0.1
ORPHEUS_SERVICE_PORT=8080
ENABLE_ORPHEUS_FEATURES=true
SHMUI_DEV_SERVER_PORT=4173
ENGINE_NATIVE_LOG_LEVEL=info
ENV
fi

echo "→ Running Phase 0 validation…"
if [ -x scripts/validate-phase0.sh ]; then
  if ! scripts/validate-phase0.sh; then
    echo "⚠️  Phase 0 validation failed. Review ${TROUBLE_URL} for next steps." >&2
    exit 1
  fi
else
  echo "→ No validation script found; skipping."
fi

trap - ERR
echo "✓ Bootstrap complete. Run 'pnpm dev' to start."
