#!/usr/bin/env bash
#
# test/gates/g5_negative.sh - prove g5 rejects a task with no red evidence and
# a record whose baseline is not an ancestor.
#
# License: MIT (see LICENSE).

set -u
here="$(cd "$(dirname "$0")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/phases/PH-99" "$tmp/reviews/PH-99"
cp "$repo/docs/phases/PH-10/card.md" "$tmp/phases/PH-99/card.md"

# 1. no evidence at all
G5_ONLY=evidence PHASES_DIR="$tmp/phases" REVIEWS_DIR="$tmp/reviews" \
    bash "$repo/tools/check.sh" g5 >"$tmp/out1" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out1" >&2; echo "FAIL g5-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'has no red evidence' "$tmp/out1" \
    || { cat "$tmp/out1" >&2; echo "FAIL g5-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected evidence/missing: %s\n' "$(head -n 1 "$tmp/out1")"

# 2. a baseline that is not a commit
for t in T1 T2 T3 T4 T5 T6 T7 T8 T9; do
    cat >"$tmp/reviews/PH-99/$t.md" <<'EOF'
# evidence
- baseline: 0000000
- 命令: bash test/run.sh p3-url-own
```
owned 1 url https://example.test/a
```
退出码: 0
green: ok
EOF
done
G5_ONLY=evidence PHASES_DIR="$tmp/phases" REVIEWS_DIR="$tmp/reviews" \
    bash "$repo/tools/check.sh" g5 >"$tmp/out2" 2>&1
rc=$?
[ "$rc" -eq 1 ] || { cat "$tmp/out2" >&2; echo "FAIL g5-negative: expected rc=1, got $rc" >&2; exit 1; }
grep -q 'not a commit' "$tmp/out2" \
    || { cat "$tmp/out2" >&2; echo "FAIL g5-negative: wrong diagnostic" >&2; exit 1; }
printf '  rejected evidence/baseline: %s\n' "$(head -n 1 "$tmp/out2")"

printf 'ok g5-negative (2 rejected cases)\n'
