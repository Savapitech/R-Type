#!/usr/bin/env bash
# Launch the editor with the repository's .venv (see docs/pages/editor.md).
# Extra system libraries extracted in .venv/syslibs (no root needed) are used if present.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [ "$(uname -s)" = "Linux" ]; then
  libs="$root/.venv/syslibs/usr/lib/x86_64-linux-gnu"
  if [ -d "$libs" ]; then
    export LD_LIBRARY_PATH="$libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  fi
  export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
fi
cd "$root"
exec "$root/.venv/bin/python" -m tools.editor "$@"
