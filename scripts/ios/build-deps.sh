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
	cp -R "$ROOT/ios/thirdparty/zlib/"* "$dir/"
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
	cp -R "$ROOT/ios/thirdparty/libpng/"* "$dir/"
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
	cp -R "$ROOT/ios/thirdparty/libjpeg/"* "$dir/"
	cd "$dir"
	./configure --host=aarch64-apple-darwin --enable-static --disable-shared \
		--prefix="$PREFIX"
	make -j"$JOBS"
	make install
	cp .libs/libjpeg.a "$PREFIX/libjpeg.a"
}

build_freetype() {
	cd "$ROOT/ios/thirdparty/freetype"
	if [ ! -f subprojects/dlg/include/dlg/output.h ]; then
		rm -rf subprojects/dlg
		git clone --depth 1 https://github.com/nyorain/dlg.git subprojects/dlg
	fi
	make distclean 2>/dev/null || true
	if [ ! -x builds/unix/configure ]; then
		./autogen.sh
	fi
	./configure --host=aarch64-apple-darwin --enable-static --disable-shared \
		--without-harfbuzz --without-brotli --prefix="$PREFIX" \
		CPPFLAGS="-I$PREFIX/include" LDFLAGS="-L$PREFIX/lib" \
		LIBPNG_CFLAGS="-I$PREFIX/include" LIBPNG_LIBS="-L$PREFIX/lib -lpng -lz"
	make -j"$JOBS"
	make install
	cp objs/.libs/libfreetype.a "$PREFIX/libfreetype2.a"
}

build_curl() {
	local dir="$BUILD/curl-src"
	local ver="8.5.0"
	rm -rf "$dir" "$BUILD/curl-$ver"
	mkdir -p "$dir"
	curl -fsSL "https://curl.se/download/curl-${ver}.tar.gz" | tar xz -C "$BUILD"
	cd "$BUILD/curl-$ver"
	./configure --host=aarch64-apple-darwin --enable-static --disable-shared \
		--disable-ldap --disable-ldaps --without-libidn2 --without-libpsl \
		--without-nghttp2 --without-zstd --without-ssl --without-libssh2 \
		--with-zlib="$PREFIX" --prefix="$PREFIX"
	make -j"$JOBS"
	make install
	cp lib/.libs/libcurl.a "$PREFIX/libcurl.a"
}

# CS:GO pins protobuf 2.5.0: generated .pb.cc files must match the runtime
# headers exactly. Build a host protoc and an iOS libprotobuf.a from the copy
# in thirdparty/.
build_protobuf() {
	local src="$ROOT/thirdparty/protobuf-2.5.0"
	local hdir="$BUILD/protobuf-host"
	local idir="$BUILD/protobuf-ios"

	rm -rf "$hdir" "$idir"
	mkdir -p "$hdir" "$idir" "$ROOT/build/host"
	cp -R "$src/"* "$hdir/"
	cp -R "$src/"* "$idir/"

	(
		cd "$hdir"
		env CC=clang CXX=clang++ CFLAGS="-O2" CXXFLAGS="-O2 -std=gnu++11" LDFLAGS="" \
			./configure --disable-shared --enable-static
		make -j"$JOBS" -C src protoc
		cp src/protoc "$ROOT/build/host/protoc"
	)

	(
		cd "$idir"
		CXXFLAGS="$CXXFLAGS -std=gnu++11" ./configure --host=aarch64-apple-darwin \
			--build="$(./config.guess)" --enable-static --disable-shared \
			--with-protoc="$ROOT/build/host/protoc"
		make -j"$JOBS" -C src libprotobuf.la
		cp src/.libs/libprotobuf.a "$PREFIX/libprotobuf.a"
	)
}

# Ensure waf's lib checks find archives in the search path root.
finalize_libs() {
	for lib in libz libpng libjpeg libcurl libfreetype2 libbz2; do
		if [ -f "$PREFIX/lib/${lib}.a" ] && [ ! -f "$PREFIX/${lib}.a" ]; then
			cp "$PREFIX/lib/${lib}.a" "$PREFIX/${lib}.a"
		fi
	done
}

build_zlib
build_bz2
build_png
build_jpeg
build_freetype
build_curl
build_protobuf
finalize_libs

echo "iOS deps installed to $PREFIX"
