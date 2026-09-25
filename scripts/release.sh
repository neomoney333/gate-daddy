#!/bin/bash
# Usage: ./scripts/release.sh 1.0.1
# Bumps the version, commits, tags and pushes. GitHub Actions then builds the
# Mac + Windows installers and publishes a Release. Every copy of Gate Daddy
# shows an UPDATE button the next time it's opened.
set -euo pipefail
cd "$(dirname "$0")/.."

NEW="${1:-}"
if [[ ! "$NEW" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo "Usage: ./scripts/release.sh X.Y.Z   (current: $(sed -n 's/^project(GateDaddy VERSION \([0-9.]*\)).*/\1/p' CMakeLists.txt))"
    exit 1
fi
if git rev-parse "v$NEW" >/dev/null 2>&1; then
    echo "Tag v$NEW already exists. Pick a higher version."
    exit 1
fi

sed -i '' "s/^project(GateDaddy VERSION [0-9.]*)/project(GateDaddy VERSION $NEW)/" CMakeLists.txt
git add -A
git commit -m "Release v$NEW"
git tag "v$NEW"
git push origin HEAD --tags
echo
echo "Pushed v$NEW. Build progress: https://github.com/neomoney333/gate-daddy/actions"
