#!/usr/bin/env bash
# Publishes the project to the Pebble app store with the listing assets
# in store/. Description and screenshots are attached here, at publish time;
# the .pbw itself carries neither.
#   tools/publish.sh [extra pebble publish args, e.g. --release-notes "..."]
# --non-interactive is what makes pebble publish use --screenshots files
# instead of capturing its own; --description only takes effect when the
# store app is first created.
# Screenshots are uploaded in the order given (five states per platform,
# portrait first, so it is the leading listing image); pass --replace-screenshots to swap an existing set.
set -euo pipefail
cd "$(dirname "$0")/.."
shots=()
for platform in aplite basalt diorite emery flint; do
  # PLATFORM_N_*.png in N order; N=1 (portrait) leads the listing.
  shots+=(store/screenshots/"${platform}"_[0-9]_*.png)
done
pebble publish \
  --description "$(cat store/description.txt)" \
  --screenshots "${shots[@]}" \
  --no-gif-all-platforms \
  --non-interactive \
  "$@"
