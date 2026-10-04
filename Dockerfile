FROM ubuntu:24.04

ARG SDL_VERSION=3.4.18
ARG SDL_URL=https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential cmake pkg-config ca-certificates curl gcc-mingw-w64-x86-64 wine64 \
        clang lld llvm openssl nodejs \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev \
        libxkbcommon-dev libwayland-dev wayland-protocols libdecor-0-dev \
        libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libdrm-dev libgbm-dev \
        libdbus-1-dev libibus-1.0-dev libudev-dev \
    && rm -rf /var/lib/apt/lists/* \
    && printf '#!/bin/sh\nexec /usr/lib/wine/wine64 "$@"\n' > /usr/local/bin/wine \
    && chmod +x /usr/local/bin/wine

RUN curl -sSL ${SDL_URL}/SDL3-${SDL_VERSION}.tar.gz | tar xz -C /tmp \
    && cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl -DCMAKE_BUILD_TYPE=Release -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DCMAKE_INSTALL_PREFIX=/opt/sdl3 \
    && cmake --build /tmp/sdl -j"$(nproc)" \
    && cmake --install /tmp/sdl \
    && rm -rf /tmp/SDL3-${SDL_VERSION} /tmp/sdl

RUN curl -sSL ${SDL_URL}/SDL3-devel-${SDL_VERSION}-mingw.tar.gz | tar xz -C /opt \
    && mv /opt/SDL3-${SDL_VERSION} /opt/sdl3-mingw

ENV WINEDEBUG=fixme-all \
    WINEPREFIX=/opt/wine \
    PKG_CONFIG_PATH=/opt/sdl3/lib/pkgconfig \
    SDL3_DIR=/opt/sdl3-mingw/x86_64-w64-mingw32

RUN wine wineboot --init; /usr/lib/wine/wineserver -w; true

WORKDIR /src
COPY . .

CMD ["sh", "tools/build_all.sh"]
