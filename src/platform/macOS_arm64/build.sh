#!/usr/bin/env bash
set -euo pipefail

repository_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cd "$repository_root"

compiler=${CXX:-clang++}
build_root=${BUILD_ROOT:-$repository_root/obj/macOS_arm64}
output_root=${OUTPUT_ROOT:-$repository_root/out/macOS_arm64}

common_flags=(
	-std=c++23
	-arch arm64
	-O2
	-DNDEBUG
	-Wno-nonportable-include-path
	-Wno-unknown-pragmas
)

compile_one() {
	local source_file=$1
	local relative_path=${source_file#src/}
	local component

	if [[ $relative_path == platform/macOS_arm64/* ]]; then
		local platform_relative=${relative_path#platform/macOS_arm64/}
		component=${platform_relative%%/*}
	else
		component=${relative_path%%/*}
	fi

	local object_file="$build_root/$source_file.o"
	local stamp_file="$object_file.sha256"
	local source_hash
	local object_hash

	source_hash=$(shasum -a 256 "$source_file" | awk '{print $1}')
	object_hash=$(printf '%s\n%s\n' "$MACOS_ARM64_BUILD_SIGNATURE" "$source_hash" | shasum -a 256 | awk '{print $1}')

	if [[ -f $object_file && -f $stamp_file && $(<"$stamp_file") == "$object_hash" ]]; then
		printf 'CACHED  %s\n' "$source_file"
		return
	fi

	mkdir -p "$(dirname "$object_file")"
	local temporary_object="$object_file.tmp.$$"
	local temporary_stamp="$stamp_file.tmp.$$"

	"$compiler" "${common_flags[@]}" \
		-Isrc/platform/macOS_arm64/"$component"/h \
		-Isrc/platform/macOS_arm64/h \
		-Isrc/"$component"/h \
		-Isrc/"$component"/source \
		-Isrc/"$component"/autogen \
		-Isrc/h \
		-c "$source_file" \
		-o "$temporary_object"

	mv "$temporary_object" "$object_file"
	printf '%s\n' "$object_hash" > "$temporary_stamp"
	mv "$temporary_stamp" "$stamp_file"
	printf 'COMPILED %s\n' "$source_file"
}

if [[ ${1:-} == --compile-one ]]; then
	compile_one "$2"
	exit 0
fi

if [[ $(uname -s) != Darwin || $(uname -m) != arm64 ]]; then
	printf 'This build requires macOS on ARM64; detected %s/%s.\n' "$(uname -s)" "$(uname -m)" >&2
	exit 1
fi

if ! command -v "$compiler" >/dev/null 2>&1; then
	printf 'C++ compiler not found: %s\n' "$compiler" >&2
	exit 1
fi

jobs=${JOBS:-$(sysctl -n hw.logicalcpu)}
if [[ ! $jobs =~ ^[1-9][0-9]*$ ]]; then
	printf 'JOBS must be a positive integer; got: %s\n' "$jobs" >&2
	exit 1
fi

mkdir -p "$build_root"

headers_hash=$(
	find src \
		-path src/platform/Windows_x64 -prune -o \
		-type f \( -name '*.h' -o -name '*.hpp' -o -name '*.inl' \) -print |
		LC_ALL=C sort |
		while IFS= read -r header_file; do
			shasum -a 256 "$header_file"
		done |
		shasum -a 256 |
		awk '{print $1}'
)

compiler_version=$($compiler --version)
build_format_version=1
MACOS_ARM64_BUILD_SIGNATURE=$(printf '%s\n%s\n%s\n%s\n' \
	"$compiler_version" \
	"${common_flags[*]}" \
	"$headers_hash" \
	"$build_format_version" |
	shasum -a 256 |
	awk '{print $1}')

export CXX="$compiler"
export BUILD_ROOT="$build_root"
export OUTPUT_ROOT="$output_root"
export MACOS_ARM64_BUILD_SIGNATURE

source_list="$build_root/sources.txt"
find src \
	-path src/platform/Windows_x64 -prune -o \
	-type f \( -name '*.cpp' -o -name '*.mm' \) -print |
	LC_ALL=C sort > "$source_list"

source_count=$(wc -l < "$source_list" | tr -d ' ')
printf 'Compiling %s C++ translation units with %s parallel jobs.\n' "$source_count" "$jobs"

tr '\n' '\0' < "$source_list" |
	xargs -0 -P "$jobs" -n 1 "$BASH_SOURCE" --compile-one

mkdir -p "$output_root"
portable_test_executable="$output_root/AltirraPortableTests"
portable_test_sources=(
	src/platform/macOS_arm64/ATTest/source/main.cpp
	src/ATTest/source/portabletests.cpp
	src/ATTest/source/TestSystem_Binary.cpp
	src/ATTest/source/TestSystem_BitMath.cpp
	src/ATTest/source/TestSystem_Constexpr.cpp
	src/ATTest/source/TestSystem_CPUAccel.cpp
	src/ATTest/source/TestSystem_Error.cpp
	src/ATTest/source/TestSystem_Fraction.cpp
	src/ATTest/source/TestSystem_Hash.cpp
	src/ATTest/source/TestSystem_HalfFloat.cpp
	src/ATTest/source/TestSystem_Int128Portable.cpp
	src/ATTest/source/TestSystem_MathPortable.cpp
	src/ATTest/source/TestSystem_Memory.cpp
	src/ATTest/source/TestSystem_RefCount.cpp
	src/ATTest/source/TestSystem_StrUtil.cpp
	src/ATTest/source/TestSystem_TLS.cpp
	src/ATTest/source/TestSystem_VDAlloc.cpp
	src/ATTest/source/TestSystem_VDFunction.cpp
	src/ATTest/source/TestSystem_VDString.cpp
	src/ATTest/source/TestSystem_VDSTL.cpp
	src/ATTest/source/TestSystem_Vectors.cpp
	src/platform/macOS_arm64/system/source/binary.cpp
	src/platform/macOS_arm64/system/source/bitmath.cpp
	src/platform/macOS_arm64/system/source/constexpr.cpp
	src/platform/macOS_arm64/system/source/cpuaccel.cpp
	src/platform/macOS_arm64/system/source/Error.cpp
	src/platform/macOS_arm64/system/source/error_macos.mm
	src/platform/macOS_arm64/system/source/Fraction.cpp
	src/platform/macOS_arm64/system/source/hash.cpp
	src/platform/macOS_arm64/system/source/halffloat.cpp
	src/platform/macOS_arm64/system/source/int128.cpp
	src/platform/macOS_arm64/system/source/math.cpp
	src/platform/macOS_arm64/system/source/memory.cpp
	src/platform/macOS_arm64/system/source/refcount.cpp
	src/platform/macOS_arm64/system/source/strutil.cpp
	src/platform/macOS_arm64/system/source/tls.cpp
	src/platform/macOS_arm64/system/source/vdalloc.cpp
	src/platform/macOS_arm64/system/source/function.cpp
	src/platform/macOS_arm64/system/source/VDString.cpp
	src/platform/macOS_arm64/system/source/vdstl.cpp
	src/platform/macOS_arm64/system/source/vectors.cpp
)
portable_test_objects=()

for test_source in "${portable_test_sources[@]}"; do
	portable_test_objects+=("$build_root/$test_source.o")
done

"$compiler" -arch arm64 "${portable_test_objects[@]}" -framework AppKit -o "$portable_test_executable"

manifest="$build_root/build-manifest.txt"
{
	printf 'platform=macOS_arm64\n'
	printf 'compiler=%s\n' "$(printf '%s\n' "$compiler_version" | head -n 1)"
	printf 'translation_units=%s\n' "$source_count"
	printf 'build_signature=%s\n' "$MACOS_ARM64_BUILD_SIGNATURE"
	printf 'portable_test_executable=%s\n' "${portable_test_executable#$repository_root/}"
	printf '\nobjects:\n'
	while IFS= read -r source_file; do
		object_file="$build_root/$source_file.o"
		if [[ ! -s $object_file ]]; then
			printf 'Missing object file: %s\n' "$object_file" >&2
			exit 1
		fi
		printf '%s\n' "${object_file#$repository_root/}"
	done < "$source_list"
} > "$manifest"

printf 'macOS ARM64 compile completed: %s objects.\n' "$source_count"
