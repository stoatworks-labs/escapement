#!/usr/bin/env python3
"""Prove the demo's preset table is the plugin's preset table.

`plugin.js` cannot include a C++ header, so `source/Presets.h` and the `PRESETS`
object in `demo/plugin.js` are two hand-written copies of one table -- same host
0..1 units, camelCase keys instead of a positional initialiser list, and the
keys the plugin marks `-1` ("no opinion, leave the operator's value alone")
simply left out.

Nothing enforces that they agree. A preset retuned in Presets.h and not mirrored
here is invisible: the plugin gets the new look, the deployed demo goes on
showing the old one, and the only symptom is that the two do not match -- which
nobody sees, because nobody runs them side by side. That happened on 2026-08-27,
when the inject level and drift on four presets were retuned and plugin.js had
to be found and fixed by hand.

This reads both and compares them value by value.

    python3 demo/tools/check_presets.py

Exit status is 0 when every preset matches, 1 otherwise, so it can go in
`tools/verify.sh`.

Two things bite when parsing Presets.h, and both are handled here rather than
trusted:

  * the comments beside the values are full of commas and numbers ("zoom 0.98 --
    BELOW one"), so `//` comments are stripped before anything is tokenised;

  * a row with fewer than `kParamCount` values is legal C++ -- the rest are
    zero-filled, silently, and a zero is a perfectly plausible parameter value.
    That trap has already cost three broken presets, so a short row is a failure
    in its own right and not something to compare around.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
PRESETS_H = ROOT / "source" / "Presets.h"
PLUGIN_JS = ROOT / "demo" / "plugin.js"

# A preset leaves a parameter alone by setting it to this, and the demo says the
# same thing by omitting the key.
NO_OPINION = -1.0

# Host values are written out to three decimals at most on both sides, so
# anything closer than this is the same number typed two ways (0.600f vs 0.6).
EPSILON = 1e-6


def strip_comments(text):
    """Remove `//` comments, leaving string literals alone.

    The values in the table are followed by comments containing commas, numbers
    and the odd `--`, all of which tokenise beautifully as parameters.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def param_names(text):
    """`enum Param` in order, as the camelCase names the demo uses.

    P_INJECT_LEVEL -> injectLevel. Derived rather than tabulated, so a parameter
    appended to the enum is covered the day it lands.
    """
    match = re.search(r"\benum Param\s*\{(.*?)\}", text, re.DOTALL)
    if not match:
        raise SystemExit("no `enum Param` in source/Presets.h")

    names = []
    for entry in match.group(1).split(","):
        entry = entry.split("=")[0].strip()
        if not entry or not entry.startswith("P_"):
            continue
        head, *rest = entry[2:].lower().split("_")
        names.append(head + "".join(word.capitalize() for word in rest))
    return names


def cpp_presets(text):
    """Every `{ "Name", { ... } }` row of `kPresets`, in table order."""
    start = re.search(r"\binline constexpr Preset kPresets\[\]\s*=\s*\{", text)
    if not start:
        raise SystemExit("no `kPresets` table in source/Presets.h")

    end = text.find("};", start.end())
    body = text[start.end():end]

    rows = []
    pattern = re.compile(r'\{\s*"([^"]*)"\s*,\s*\{([^{}]*)\}\s*\}')
    for match in pattern.finditer(body):
        values = []
        for token in match.group(2).split(","):
            token = token.strip().rstrip("fF")
            if token:
                values.append(float(token))
        rows.append((match.group(1), values))
    return rows


def js_presets(text):
    """The `PRESETS` object of demo/plugin.js: name -> {key: value}."""
    match = re.search(r"^const PRESETS = \{$(.*?)^\};$", text, re.DOTALL | re.MULTILINE)
    if not match:
        raise SystemExit("no `const PRESETS = {` in demo/plugin.js")

    rows = {}
    entry = re.compile(r"^\s*'([^']+)':\s*\{(.*)\},\s*$")
    field = re.compile(r"(\w+)\s*:\s*(-?[0-9.]+)")
    for line in match.group(1).splitlines():
        found = entry.match(line)
        if not found:
            continue
        rows[found.group(1)] = {
            f.group(1): float(f.group(2)) for f in field.finditer(found.group(2))
        }
    return rows


def compare(name, params, values, demo):
    """The differences between one plugin row and one demo row, as strings."""
    problems = []

    for index, key in enumerate(params):
        want = values[index]
        has = key in demo

        if abs(want - NO_OPINION) < EPSILON:
            if has:
                problems.append(
                    f"{key}: Presets.h leaves it alone, plugin.js sets {demo[key]:g}"
                )
            continue

        if not has:
            problems.append(f"{key}: Presets.h has {want:g}, plugin.js omits it")
        elif abs(want - demo[key]) > EPSILON:
            problems.append(f"{key}: Presets.h has {want:g}, plugin.js has {demo[key]:g}")

    for key in demo:
        if key not in params:
            problems.append(f"{key}: plugin.js has it, and it is not a Param")

    return problems


def main():
    header = strip_comments(PRESETS_H.read_text(encoding="utf-8"))

    params = param_names(header)
    plugin = cpp_presets(header)
    demo = js_presets(PLUGIN_JS.read_text(encoding="utf-8"))

    failures = 0

    for name, values in plugin:
        # A short initialiser list is legal C++ and zero-fills the rest without
        # a word from the compiler, so the count is checked before the values
        # are worth comparing at all.
        if len(values) != len(params):
            print(f"SHORT    {name} has {len(values)} values, expected {len(params)}")
            if len(values) < len(params):
                print(f"           C++ zero-fills the rest -- first missing is "
                      f"{params[len(values)]}")
            failures += 1
            continue

        if name not in demo:
            print(f"MISSING  {name} not found in demo/plugin.js")
            failures += 1
            continue

        problems = compare(name, params, values, demo[name])
        if not problems:
            print(f"ok       {name}")
            continue

        failures += 1
        print(f"DRIFTED  {name}")
        for problem in problems:
            print(f"           {problem}")

    for name in demo:
        if not any(name == row[0] for row in plugin):
            print(f"EXTRA    {name} is in demo/plugin.js and not in source/Presets.h")
            failures += 1

    if failures:
        print(f"\n{failures} preset(s) drifted. "
              f"The demo is no longer showing the plugin's presets.")
        return 1

    print(f"\nall {len(plugin)} preset(s) identical "
          f"across {len(params)} parameters")
    return 0


if __name__ == "__main__":
    sys.exit(main())
