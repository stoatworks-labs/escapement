#!/usr/bin/env bash
#
# Everything, in the order that fails fastest.
#
# The build is universal on purpose. An arm64-only bundle builds and tests
# perfectly well here and then fails to load in an Intel Resolume, and the build
# log calls it a success either way -- so the architecture is checked with lipo,
# never with the log.
#
#     tools/verify.sh [BUILD_DIR]
#
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:-$REPO/build-verify}"

cd "$REPO"

step() { printf '\n\033[1m== %s\033[0m\n' "$1"; }
fail() { printf '\033[31mFAIL\033[0m %s\n' "$1"; exit 1; }

#---------------------------------------------------------------------------
# Every shader, through a real GLSL compiler, before a host has to find out.
#
# A shader that will not compile presents to an operator as "the effect does
# nothing", with the real message buried in the diagnostics log -- so without
# this it is caught at run time, in a host, or not at all.
#
# glslc is optional -- `brew install shaderc` -- so a machine without it skips
# rather than fails.
#---------------------------------------------------------------------------
shaders_compile() {
	local dir bad=0 n=0 shader

	if ! command -v glslc >/dev/null 2>&1; then
		printf '   skipped: glslc not installed (brew install shaderc)\n'
		return 0
	fi

	dir="$( mktemp -d )"

	python3 - "$dir" <<'SHADERS_PY'
import re, sys, pathlib
out = pathlib.Path( sys.argv[ 1 ] )

FILES = [ "source/Shaders.cpp" ]

# The effect build, which Escapement.cpp splices into two of the three passes.
VARIANTS = {
	"kLoopFragmentShader":    [ ( "effect", "#define ESCAPEMENT_EFFECT 1\n" ) ],
	"kDisplayFragmentShader": [ ( "effect", "#define ESCAPEMENT_EFFECT 1\n" ) ],
}

pattern = re.compile( r'const char\* const (\w+)\s*=\s*R"\((.*?)\)";', re.S )

for name in FILES:
	text = pathlib.Path( name ).read_text()
	for match in pattern.finditer( text ):
		symbol, source = match.group( 1 ), match.group( 2 )
		stage = "vert" if "VertexShader" in symbol else "frag"

		builds = [ ( "", "" ) ] + VARIANTS.get( symbol, [] )
		for suffix, define in builds:
			body = source
			if define:
				nl = body.find( "\n" )
				body = body[ :nl + 1 ] + define + body[ nl + 1: ]

			leaf = symbol + ( "." + suffix if suffix else "" ) + "." + stage
			( out / leaf ).write_text( body )

SHADERS_PY

	for shader in "$dir"/*; do
		n=$(( n + 1 ))
		# --target-env=opengl4.5 with -fauto-map-locations: glslc targets SPIR-V,
		# which demands an explicit layout( location ) on every uniform and
		# varying. Those are Vulkan rules and not GLSL ones, and without the flag
		# every shader "fails" for reasons that have nothing to do with the code.
		if ! glslc --target-env=opengl4.5 -fauto-map-locations \
			-fshader-stage="${shader##*.}" "$shader" -o /dev/null 2>"$dir/err"; then
			printf '\n'
			cat "$dir/err"
			bad=$(( bad + 1 ))
		fi
	done

	rm -rf "$dir"
	[ "$bad" -eq 0 ] || fail "$bad shader(s) would not compile"
	printf '   %d shader builds compiled\n' "$n"
}

step "shaders"
shaders_compile

step "build (universal)"
cmake -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD" -j"$( sysctl -n hw.ncpu )" >/dev/null
printf '   built\n'

#---------------------------------------------------------------------------
# lipo, not the build log. CMAKE_OSX_ARCHITECTURES is latched when the first
# target is created; set too late it is silently ignored and the build reports
# success either way.
#---------------------------------------------------------------------------
step "architectures"
for bundle in "Escapement" "Escapement Feed"; do
	binary="$BUILD/$bundle.bundle/Contents/MacOS/$bundle"
	[ -f "$binary" ] || fail "no binary at $binary"

	archs="$( lipo -archs "$binary" )"
	case "$archs" in
		*arm64*) ;;
		*) fail "$bundle is missing arm64 (has: $archs)" ;;
	esac
	case "$archs" in
		*x86_64*) ;;
		*) fail "$bundle is missing x86_64 (has: $archs)" ;;
	esac
	printf '   %-22s %s\n' "$bundle" "$archs"
done

#---------------------------------------------------------------------------
# A bundle that loads, exports plugMain and contains no plugins is the failure
# mode of putting the registration in a STATIC library. Check the symbol.
#---------------------------------------------------------------------------
step "registration"
for bundle in "Escapement" "Escapement Feed"; do
	binary="$BUILD/$bundle.bundle/Contents/MacOS/$bundle"
	nm -gU "$binary" | grep -q plugMain || fail "$bundle does not export plugMain"
	printf '   %-22s exports plugMain\n' "$bundle"
done

step "harness"
"$BUILD/esctest" --all

#---------------------------------------------------------------------------
# The demo's GLSL is the plugin's GLSL.
#
# plugin.js cannot include a C++ file, so the two copies are two files that
# happen to agree — and a change to Shaders.cpp that is not mirrored is
# invisible until somebody notices the demo behaving differently from the
# plugin. This compares them character for character.
#---------------------------------------------------------------------------
step "demo shaders"
python3 demo/tools/check_shaders.py

step "no dead controls"
python3 tools/sweep.py --build "$( basename "$BUILD" )"

printf '\n\033[32mall checks passed\033[0m\n'
