#!/usr/bin/env bash
#
# Validate a single commit subject line against Conventional Commits as
# enforced by @commitlint/config-conventional:
# https://github.com/conventional-changelog/commitlint/blob/master/%40commitlint/config-conventional/src/index.ts
#
#   - type-enum / type-case:     <type> must be one of the allowed, lowercase types
#   - scope-case:                <scope>, if present, must be lowercase
#   - subject-empty:              a description is required
#   - subject-case:               description must not start with an uppercase
#                                  letter (blocks sentence-case/start-case/
#                                  Pascal-case/UPPER-CASE)
#   - subject-full-stop:          description must not end with "."
#   - header-max-length:           whole header must be <= 100 characters
#
# Usage: check-commit-msg.sh "<commit subject>"
# Exit status: 0 if valid (or a Merge/Revert commit), 1 otherwise.

set -euo pipefail

subject_line="${1:-}"

# Let git-generated merge/revert commits through untouched.
if [[ "$subject_line" =~ ^(Merge|Revert) ]]; then
    exit 0
fi

types="feat|fix|docs|style|refactor|perf|test|build|ci|chore|revert"
pattern="^(${types})(\(([a-zA-Z0-9_/.,-]+)\))?!?: (.+)$"

fail() {
    echo "Error: commit message does not follow Conventional Commits." >&2
    echo "" >&2
    echo "  Got:    \"${subject_line}\"" >&2
    echo "  Reason: $1" >&2
    echo "" >&2
    echo "  Expected: <type>(<scope>): <description>" >&2
    echo "  Types:    ${types//|/, }" >&2
    echo "  Rules:    scope lowercase, description not capitalized, no trailing '.', <=100 chars" >&2
    echo "" >&2
    echo "  Example:  fix(server/game): handle missing team name" >&2
    exit 1
}

if (( ${#subject_line} > 100 )); then
    fail "header is longer than 100 characters"
fi

if ! [[ "$subject_line" =~ $pattern ]]; then
    fail "does not match <type>(<scope>): <description>, or type/scope is not lowercase"
fi

subject="${BASH_REMATCH[4]}"

if [[ "$subject" =~ ^[A-Z] ]]; then
    fail "description must not start with an uppercase letter"
fi

if [[ "$subject" == *. ]]; then
    fail "description must not end with a full stop"
fi
