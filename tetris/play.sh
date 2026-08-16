#!/bin/sh
# Builds the p6 compiler and Tetris inside a Linux container, then runs Tetris in this terminal.
#   ./play.sh          build (if needed) and play
#   ./play.sh build    only build
# Needs Docker and the course tarballs (libcdk21-*.tar.bz2, librts7-*.tar.bz2) in $COURSE_DIR (default ~/Downloads).
set -e
here=$(cd "$(dirname "$0")" && pwd)
repo=$(dirname "$here")
course=${COURSE_DIR:-$HOME/Downloads}
image=p6-toolchain

if ! docker image inspect "$image" >/dev/null 2>&1; then
  mkdir -p "$here/docker/vendor"
  cp "$course"/libcdk21-*.tar.bz2 "$course"/librts7-*.tar.bz2 "$here/docker/vendor/"
  docker build --platform linux/amd64 -t "$image" "$here/docker"
fi

run() { docker run --rm --platform linux/amd64 -v "$repo:/work" "$@"; }

run -w /work "$image" sh -c 'make -s && make -s -C tetris'

[ "$1" = build ] && exit 0
run -it -w /work/tetris "$image" ./tetris
