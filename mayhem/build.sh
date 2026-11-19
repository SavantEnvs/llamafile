#!/usr/bin/env bash
#
# mayhem/build.sh — build llamafile's own input parsers natively with clang + libFuzzer.
#
# Upstream builds with cosmocc (Cosmopolitan) and GNU make, and pulls all of llama.cpp /
# whisper.cpp in as submodules. The code that parses untrusted bytes in llamafile itself is
# small and self-contained, so this compiles those upstream sources directly against a thin
# <cosmo.h> shim (mayhem/shim) instead:
#   llamafile/llamafile.c + zip.c        weights container (GGUF / zip / .llamafile)
#   llamafile/highlight/*                terminal syntax highlighter (+ gperf keyword tables)
#   llamafile/datauri.cpp + string.cpp   data: URI parser
#   llamafile/image.cpp + xterm.cpp      image sniffing / stb decode / ANSI rendering
#   third_party/stb                      stb_image, stb_image_resize2 (vendored upstream)
# The one file from the llama.cpp submodule that datauri/image need (common/base64.hpp) is
# cached in the image at /opt/toolchains/deps/llama.cpp-base64-8642541b (mayhem/Dockerfile).
#
# Outputs (all under the gitignored /o tree):
#   o/mayhem/fuzz/<target>, o/mayhem/fuzz/<target>-standalone   sanitized fuzz binaries
#   o/mayhem/test/*                                              normal-flag test/oracle binaries
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${BASE64_DIR:=/opt/toolchains/deps/llama.cpp-base64-8642541b/include}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS
cd "$SRC"

[ -f "$BASE64_DIR/base64.hpp" ] || { echo "missing $BASE64_DIR/base64.hpp" >&2; exit 1; }

OUT="$SRC/o/mayhem"
GEN="$OUT/gen"
mkdir -p "$OUT/fuzz" "$OUT/test" "$GEN"

# 0) gperf keyword tables -> C (same invocation as build/rules.mk, minus the APE wrapper).
#    Regenerate only when the .gperf is newer, so a re-run is cheap and idempotent.
for g in llamafile/highlight/*.gperf; do
  c="$GEN/$(basename "${g%.gperf}").c"
  if [ ! -f "$c" ] || [ "$g" -nt "$c" ]; then
    gperf --output-file="$c.tmp" "$g" && mv "$c.tmp" "$c"
  fi
done

HL_CPP=$(ls llamafile/highlight/*.cpp | grep -v '_test\.cpp$')
LIB_C="llamafile/llamafile.c llamafile/zip.c"
LIB_CPP="$HL_CPP llamafile/datauri.cpp llamafile/string.cpp llamafile/image.cpp llamafile/xterm.cpp"
STB_C="third_party/stb/stb_image.c third_party/stb/stb_image_resize2.c"
INC=(-include "$SRC/mayhem/shim/cosmo.h" -I"$SRC/mayhem/shim" -iquote "$SRC/llamafile"
     -iquote "$SRC/llamafile/highlight" -I"$SRC/third_party" -I"$BASE64_DIR" -D_GNU_SOURCE)

# build_lib <dir> <cflags...>: compile every library source into <dir>/liblf.a
build_lib() {
  local dir="$1"; shift
  local flags=("$@")
  mkdir -p "$dir/obj"
  local objs=() jobs=0 f o
  for f in $LIB_C $STB_C "$GEN"/*.c mayhem/shim/cosmo_shim.c; do
    o="$dir/obj/$(echo "$f" | tr '/' '_').o"; objs+=("$o")
    # stb_image_resize2 indexes its sRGB table through a deliberately biased pointer
    # (fp32_to_srgb8_tab4 - (127-13)*8), which UBSan's array-bounds check flags on every
    # image; only that check is relaxed, only for this file (ASan still checks the access).
    local cextra=(); [ "$f" != third_party/stb/stb_image_resize2.c ] || cextra=(-fno-sanitize=array-bounds)
    $CC "${flags[@]}" "${INC[@]}" "${cextra[@]}" -w -c "$f" -o "$o" &
    jobs=$((jobs+1)); [ "$jobs" -lt "$MAYHEM_JOBS" ] || { wait -n; jobs=$((jobs-1)); }
  done
  for f in $LIB_CPP; do
    o="$dir/obj/$(echo "$f" | tr '/' '_').o"; objs+=("$o")
    local extra=(); [ "$f" != llamafile/highlight/util.cpp ] || extra=(-DLF_SHIM_TPENC)
    $CXX -std=gnu++20 "${flags[@]}" "${INC[@]}" "${extra[@]}" -w -c "$f" -o "$o" &
    jobs=$((jobs+1)); [ "$jobs" -lt "$MAYHEM_JOBS" ] || { wait -n; jobs=$((jobs-1)); }
  done
  while [ "$jobs" -gt 0 ]; do wait -n; jobs=$((jobs-1)); done
  rm -f "$dir/liblf.a"
  ar rcs "$dir/liblf.a" "${objs[@]}"
}

# 1) The project, sanitized + DWARF-3, for the fuzzers.
FZ_FLAGS=($SANITIZER_FLAGS $DEBUG_FLAGS -O1 -fsanitize=fuzzer-no-link)
build_lib "$OUT/fuzzlib" "${FZ_FLAGS[@]}"

# 2) Harnesses: a libFuzzer binary and a -standalone reproducer each.
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c mayhem/lsan_off.cc -o "$OUT/fuzzlib/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OUT/fuzzlib/standalone_main.o"
for h in mayhem/harnesses/fuzz_*.c mayhem/harnesses/fuzz_*.cpp; do
  name=$(basename "${h%.*}")
  case "$h" in
    *.c)   $CC  "${FZ_FLAGS[@]}" "${INC[@]}" -c "$h" -o "$OUT/fuzzlib/$name.o" ;;
    *.cpp) $CXX -std=gnu++20 "${FZ_FLAGS[@]}" "${INC[@]}" -c "$h" -o "$OUT/fuzzlib/$name.o" ;;
  esac
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OUT/fuzzlib/$name.o" \
    "$OUT/fuzzlib/lsan_off.o" "$OUT/fuzzlib/liblf.a" -lm -o "$OUT/fuzz/$name"
  $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OUT/fuzzlib/standalone_main.o" "$OUT/fuzzlib/$name.o" \
    "$OUT/fuzzlib/lsan_off.o" "$OUT/fuzzlib/liblf.a" -lm -o "$OUT/fuzz/$name-standalone"
done

# 3) Test suite + oracle tools with normal flags (no sanitizers).
TS_FLAGS=(-O2 $COVERAGE_FLAGS)
build_lib "$OUT/testlib" "${TS_FLAGS[@]}"
for t in highlight_c_test highlight_python_test highlight_test; do
  $CXX -std=gnu++20 "${TS_FLAGS[@]}" "${INC[@]}" -w "llamafile/highlight/$t.cpp" \
    "$OUT/testlib/liblf.a" -lm -o "$OUT/test/$t"
done
$CC "${TS_FLAGS[@]}" "${INC[@]}" mayhem/oracle/lf_open.c "$OUT/testlib/liblf.a" -o "$OUT/test/lf_open"
$CXX -std=gnu++20 "${TS_FLAGS[@]}" "${INC[@]}" mayhem/oracle/lf_highlight.cpp "$OUT/testlib/liblf.a" -o "$OUT/test/lf_highlight"
$CXX -std=gnu++20 "${TS_FLAGS[@]}" "${INC[@]}" mayhem/oracle/lf_datauri.cpp "$OUT/testlib/liblf.a" -o "$OUT/test/lf_datauri"
echo "build.sh: done"
ls -1 "$OUT/fuzz"
