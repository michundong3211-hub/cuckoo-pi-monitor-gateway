#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
WEB_ASSETS_DIR="${SCRIPT_DIR}/../web-ui/dist/assets"
APP_JS_SOURCE="${WEB_ASSETS_DIR}/index.js.gz"
APP_CSS_SOURCE="${WEB_ASSETS_DIR}/style.css.gz"
APP_JS_TARGET="${SCRIPT_DIR}/html/app.js.gz"
APP_CSS_TARGET="${SCRIPT_DIR}/html/app.css.gz"

for source in "${APP_JS_SOURCE}" "${APP_CSS_SOURCE}"; do
  if [[ ! -f "${source}" ]]; then
    printf 'Missing UI asset: %s\n' "${source}" >&2
    exit 1
  fi
done

mkdir -p "${SCRIPT_DIR}/html"
cp -- "${APP_JS_SOURCE}" "${APP_JS_TARGET}"
cp -- "${APP_CSS_SOURCE}" "${APP_CSS_TARGET}"
