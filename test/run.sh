#!/usr/bin/env bash
#
# test/run.sh - run one H1/H2... fixture.
#
#   bash test/run.sh h1-t1
#
# A fixture is test/fixtures/<task-id>.ct plus <task-id>.expected.out. Neither
# exists yet: this script exits 2 for every task-id instead of reporting a
# vacuous pass.
#
# License: MIT (see LICENSE).

set -u

task="${1:-}"
if [ -z "$task" ]; then
    echo "usage: bash test/run.sh <task-id>" >&2
    exit 2
fi

here="$(cd "$(dirname "$0")" && pwd)"
src="$here/fixtures/$task.ct"
exp="$here/fixtures/$task.expected.out"

if [ ! -f "$src" ] || [ ! -f "$exp" ]; then
    echo "no fixture for '$task' (expected $src and $exp); nothing ran" >&2
    exit 2
fi

echo "$task: fixture exists but the runner is not wired yet (H1-T1)" >&2
exit 2
