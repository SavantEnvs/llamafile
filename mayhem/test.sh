#!/usr/bin/env bash
#
# mayhem/test.sh — RUN llamafile's own highlighter unit tests plus known-answer checks of the
# fuzzed parsers (all built by mayhem/build.sh with normal flags under o/mayhem/test).
#   upstream: highlight_c_test, highlight_python_test, highlight_test (llamafile/highlight)
#   known-answer: the expected outputs below are taken from upstream's own tests
#   (highlight_c_test.cpp, tests/extract_data_uris_test.cpp) or from the fixture contents.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

T="$SRC/o/mayhem/test"
for b in highlight_c_test highlight_python_test highlight_test lf_open lf_highlight lf_datauri; do
  [ -x "$T/$b" ] || { echo "missing $T/$b — mayhem/build.sh did not build it" >&2; emit_ctrf llamafile-tests 0 1; exit 1; }
done

passed=0 failed=0
pass() { echo "PASS: $1"; passed=$((passed+1)); }
fail() { echo "FAIL: $1"; failed=$((failed+1)); }

# upstream unit tests: silent on success, print a diff and exit 1 on failure
for b in highlight_c_test highlight_python_test highlight_test; do
  if "$T/$b"; then pass "$b"; else fail "$b"; fi
done

# expect <name> <expected> <cmd...>: stdout of cmd must equal expected exactly
expect() {
  local name="$1" want="$2"; shift 2
  local got; got=$("$@" 2>/dev/null; printf x); got="${got%x}"
  if [ "$got" == "$want" ]; then pass "$name"; else fail "$name"; printf '  want: %q\n  got:  %q\n' "$want" "$got"; fi
}
E=$'\033'
GGUF_BODY=$'GGUF\003'

# weights container: zip central-directory walk, zip64, member selection, raw GGUF
lf_open_check() {  # prints size + a hex dump of the entry so NUL bytes compare exactly
  "$T/lf_open" "$1" | od -An -c | tr -s ' ' | tr -d '\n'
}
GG=" s i z e = 3 9 \n G G U F 003 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 \0 h e l l o l l a m a f i l e \n"
expect zip_first_gguf "$GG" lf_open_check mayhem/testdata/model.zip
expect zip64_entry "$GG" lf_open_check mayhem/testdata/model64.zip
expect zip_member_by_name "$GG" lf_open_check mayhem/testdata/model.zip@model.gguf
expect zip_member_other " s i z e = 6 \n n o t m e \n" lf_open_check mayhem/testdata/model.zip@notes.txt
expect raw_gguf_size " s i z e = 3 9 \n \n" lf_open_check mayhem/testdata/raw.gguf
expect zip_missing_member "open failed" bash -c "\"$T/lf_open\" mayhem/testdata/model.zip@absent.gguf | tr -d '\n'"

# highlighter known answers (from highlight_c_test.cpp) + markdown fence dispatch
expect hl_c_keyword "${E}[1;34mvolatile${E}[0m" bash -c "printf volatile | \"$T/lf_highlight\" c"
expect hl_c_comment "yo ${E}[31m/* hi */${E}[0m there" bash -c "printf 'yo /* hi */ there' | \"$T/lf_highlight\" c"
expect hl_python_def "${E}[1;34mdef${E}[0m f():" bash -c "printf 'def f():' | \"$T/lf_highlight\" python"
expect hl_unknown_lang "" bash -c "printf x | \"$T/lf_highlight\" no-such-language"

# data: URI parser (cases mirror tests/extract_data_uris_test.cpp)
expect datauri_base64 $'len=40 mime=text/plain charset=utf-8 data=hello\n' "$T/lf_datauri" "text/plain;charset=utf-8;base64,aGVsbG8="
expect datauri_percent $'len=21 mime=text/plain charset= data=hi there\n' "$T/lf_datauri" "text/plain,hi%20there"

emit_ctrf llamafile-tests "$passed" "$failed"
