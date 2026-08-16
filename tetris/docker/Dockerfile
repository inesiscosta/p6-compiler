# The course toolchain (CDK21 + RTS7) on 32/64-bit x86 Linux, installed where the Makefiles expect it.
# Built by ../play.sh, which copies the two course tarballs into ./vendor first.
FROM ubuntu:24.04

RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      g++-14 gcc-14-multilib g++-14-multilib make flex bison yasm binutils python3 bzip2 libfl-dev \
 && rm -rf /var/lib/apt/lists/* \
 && ln -s /usr/bin/gcc-14 /usr/local/bin/gcc && ln -s /usr/bin/gcc-14 /usr/local/bin/cc \
 && ln -s /usr/bin/g++-14 /usr/local/bin/g++ && ln -s /usr/bin/g++-14 /usr/local/bin/c++

COPY vendor/ /tmp/vendor/

# CDK: skip the doxygen docs step (touch its marker) so doxygen isn't needed.
RUN mkdir -p /tmp/build && cd /tmp/build \
 && tar -xjf /tmp/vendor/libcdk21-*.tar.bz2 && tar -xjf /tmp/vendor/librts7-*.tar.bz2 \
 && cd /tmp/build/libcdk21-* && mkdir -p doc && touch doc/.docsdone && make -j"$(nproc)" install \
 && cd /tmp/build/librts7-* && make install \
 && rm -rf /tmp/build /tmp/vendor

ENV TERM=xterm-256color
WORKDIR /work
