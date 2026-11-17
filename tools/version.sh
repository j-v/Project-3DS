#!/bin/sh
#
# Prints the semantic version of the current checkout.
#
# The git tags are the only source of truth. A release tag is vMAJOR.MINOR.PATCH
# with an optional pre-release suffix, and the output never carries the v
# prefix: v0.1.0 becomes 0.1.0.
#
# A checkout that is not exactly on a release tag gets a semver-compatible
# development version, so that the version baked into the SMDH still says
# which code was built:
#
#   exactly on v0.1.0                 0.1.0
#   three commits after v0.1.0        0.1.1-dev.3+g3a97fc3
#   the same tree with local changes  0.1.1-dev.3+g3a97fc3.dirty
#   no tag yet                        0.0.0-dev.1+g3a97fc3
#   outside a git checkout            0.0.0-dev
#
# The release tag itself must not contain build metadata; metadata is only
# added here for builds that are not a release.
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)

# Build metadata is allowed in the output, but not in a release tag
CORE='(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)'
PRE='-((0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)(\.(0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*))*)'
BUILD='\+[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*'
TAG_SEMVER="^${CORE}(${PRE})?$"
SEMVER="^${CORE}(${PRE})?(${BUILD})?$"

# The release workflow uses this to reject a tag before it builds anything
if [ "${1:-}" = "--check-tag" ]; then
    if [ $# -ne 2 ]; then
        echo "usage: version.sh [--check-tag TAG]" >&2
        exit 2
    fi
    tag=$2
    case "$tag" in
        v*) ;;
        *)
            echo "version.sh: release tags must start with v: $tag" >&2
            exit 1
            ;;
    esac
    if ! printf '%s\n' "${tag#v}" | grep -Eq "$TAG_SEMVER"; then
        echo "version.sh: not a semantic version tag: $tag" >&2
        exit 1
    fi
    exit 0
fi

is_git_checkout() {
    git -C "$ROOT" rev-parse --git-dir > /dev/null 2>&1
}

is_dirty() {
    [ -n "$(git -C "$ROOT" status --porcelain)" ]
}

describe=""
if is_git_checkout; then
    # --long always gives us "<tag>-<commits>-g<sha>", even on the tag itself,
    # which keeps the parsing below in one shape
    describe=$(git -C "$ROOT" describe --tags --long --match 'v[0-9]*' 2> /dev/null) || describe=""
fi

if [ -n "$describe" ]; then
    sha=${describe##*-g}
    prefix=${describe%-*}
    count=${prefix##*-}
    tag=${prefix%-*}

    case "$tag" in
        v*) ;;
        *)
            echo "version.sh: the nearest tag is not a release tag: $tag" >&2
            exit 1
            ;;
    esac

    base=${tag#v}
    if ! printf '%s\n' "$base" | grep -Eq "$TAG_SEMVER"; then
        echo "version.sh: the nearest tag is not a semantic version: $tag" >&2
        exit 1
    fi

    if [ "$count" -eq 0 ]; then
        version=$base
    else
        core=${base%%-*}
        pre=""
        case "$base" in
            *-*) pre=${base#*-} ;;
        esac

        major=${core%%.*}
        rest=${core#*.}
        minor=${rest%%.*}
        patch=${rest#*.}

        if [ -n "$pre" ]; then
            # After v0.2.0-rc.1, the next builds are later candidates of the
            # same pre-release
            version="${core}-${pre}.dev.${count}"
        else
            # The next version is only known when it is tagged, so unreleased
            # builds use the next patch
            version="${major}.${minor}.$((patch + 1))-dev.${count}"
        fi
        version="${version}+g${sha}"
    fi

    if is_dirty; then
        case "$version" in
            *+*) version="${version}.dirty" ;;
            *) version="${version}+dirty" ;;
        esac
    fi
elif is_git_checkout && git -C "$ROOT" rev-parse --verify HEAD > /dev/null 2>&1; then
    count=$(git -C "$ROOT" rev-list --count HEAD)
    sha=$(git -C "$ROOT" rev-parse --short HEAD)
    version="0.0.0-dev.${count}+g${sha}"
    if is_dirty; then
        version="${version}.dirty"
    fi
else
    version="0.0.0-dev"
fi

if ! printf '%s\n' "$version" | grep -Eq "$SEMVER"; then
    echo "version.sh: produced an invalid semantic version: $version" >&2
    exit 1
fi

printf '%s\n' "$version"
