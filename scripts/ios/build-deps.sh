#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/../.."
ROOT="$PWD"
PREFIX="$ROOT/lib/darwin/aarch64"
BUILD="$ROOT/build/ios/deps"
SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"

mkdir -p "$PREFIX" "$BUILD"

export CC="clang -arch arm64 -isysroot $SDK -mios-version-min=12.0"
export CXX="clang++ -arch arm64 -isysroot $SDK -mios-version-min=12.0"
export AR="ar"
export RANLIB="ranlib"
export CFLAGS="-O2 -fPIC"
export CXXFLAGS="-O2 -fPIC"
export LDFLAGS="-arch arm64 -isysroot $SDK -mios-version-min=12.0"

build_zlib() {
	local dir="$BUILD/zlib"
	rm -rf "$dir"
	mkdir -p "$dir"
	cp -R "$ROOT/thirdparty/zlib/"* "$dir/"
	cd "$dir"
	CHOST=aarch64-apple-darwin ./configure --static --prefix="$PREFIX"
	make -j"$JOBS"
	make install
}

build_bz2() {
	cd "$ROOT/utils/bzip2"
	$CC $CFLAGS -c blocksort.c bzlib.c compress.c crctable.c decompress.c huffman.c randtable.c
	$AR rcs "$PREFIX/libbz2.a" *.o
	rm -f *.o
}

build_png() {
	local dir="$BUILD/libpng"
	rm -rf "$dir"
	mkdir -p "$dir"
	cp -R "$ROOT/thirdparty/libpng/"* "$dir/"
	cd "$dir"
	./configure --host=aarch64-apple-darwin --enable-static --disable-shared \
		--prefix="$PREFIX" CPPFLAGS="-I$PREFIX/include" LDFLAGS="-L$PREFIX/lib"
	make -j"$JOBS"
	make install
	cp .libs/libpng16.a "$PREFIX/libpng.a"
}

build_jpeg() {
	local dir="$BUILD/libjpeg"
	rm -rf "$dir"
	mkdir -p "$dir"
	cp -R "$ROOT/thirdparty/libjpeg/"* "$dir/"
	cd "$dir"
	./configure --host=aarch64-apple-darwin --enable-static --disable-shared \
		--prefix="$PREFIX"
	make -j"$JOBS"
	make install
	cp .libs/libjpeg.a "$PREFIX/libjpeg.a"
}

build_freetype() {
	local dir="$BUILD/freetype"
	rm -rf "$dir"
	mkdir -p "$dir"
	cp -R "$ROOT/thirdparty/freetype/"* "$dir/"
	cd "$dir"
	./configure --host=aarch64-apple-darwin --enable-static --disable-shared \
		--without-harfbuzz --without-brotli --prefix="$PREFIX" \
		CPPFLAGS="-I$PREFIX/include" LDFLAGS="-L$PREFIX/lib" LIBPNG_CFLAGS="-I$PREFIX/include" LIBPNG_LIBS="-L$PREFIX/lib -lpng -lz"
	make -j"$JOBS"
	make install
	cp objs/.libs/libfreetype.a "$PREFIX/libfreetype2.a"
}

build_curl() {
	local dir="$BUILD/curl"
	rm -rf "$dir"
	mkdir -p "$dir"
	cp -R "$ROOT/thirdparty/curl/"* "$dir/"
	cd "$dir"
	./configure --host=aarch64-apple-darwin --enable-static --disable-shared \
		--disable-ldap --disable-ldaps --without-libidn2 --without-libpsl \
		--without-nghttp2 --without-zstd --with-zlib="$PREFIX" --prefix="$PREFIX"
	make -j"$JOBS"
	make install
	cp lib/.libs/libcurl.a "$PREFIX/libcurl.a"
}

build_zlib
build_bz2
build_png
build_jpeg
build_freetype
build_curl

echo "iOS deps installed to $PREFIX"
