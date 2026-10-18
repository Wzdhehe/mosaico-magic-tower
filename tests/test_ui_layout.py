#!/usr/bin/env python3
"""Guard the authored UI copy against silent clipping.

The Chinese NPC dialog has two layouts: 24 px text at 11 characters per line
(up to 5 lines in the text column) and a 16 px fallback at 17 characters per
line (up to 7 lines).  The game picks the large layout when it fits and the
compact one only when needed; beyond that it deliberately truncates and this
test fails so the copy must be edited instead of hidden by an unreadable font.
"""
import math
import re
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / "main" / "game_view.c"
ZH_LARGE_CHARS = 11
ZH_SMALL_CHARS = 17
ZH_LARGE_MAX_LINES = 5
ZH_SMALL_MAX_LINES = 7
ZH_LARGE_ADVANCE = 25
ZH_SMALL_ADVANCE = 17
ZH_LARGE_INK = 24
ZH_SMALL_INK = 16
ZH_LINE_HEIGHT_LARGE = 28
ZH_LINE_HEIGHT_SMALL = 20
ZH_TEXT_Y = 174
ZH_BUTTON_Y = 330
TEXT_AREA_PX = 300
CLOSING_PUNCTUATION = set("。，！？；：、）】》」』…")
EN_CHARS = 24
EN_MAX_LINES = 6
NAME_MAX_CHARS = 4
NAME_MAX_PX = 100
URL_MAX_PX = 320


def story_strings():
    text = SOURCE.read_text(encoding="utf-8")
    block = text.split("static const story_t STORY", 1)[1].split("};", 1)[0]
    strings = re.findall(r'"([^"]*)"', block)
    assert len(strings) == 40, f"expected 40 story strings, found {len(strings)}"
    return [strings[i * 4:i * 4 + 4] for i in range(10)]


def name_strings():
    text = SOURCE.read_text(encoding="utf-8")
    block = text.split("static const text_t NPC_NAME", 1)[1].split("};", 1)[0]
    zh = re.findall(r'\{"([^"]*)"', block)
    assert len(zh) == 10, f"expected 10 NPC names, found {len(zh)}"
    return zh


def wrap_cjk(text, max_chars):
    """Mirror ui_draw_wrap_cjk: closing punctuation may overhang a line."""
    lines = []
    current = []
    for char in text:
        if len(current) >= max_chars and char not in CLOSING_PUNCTUATION:
            lines.append("".join(current))
            current = []
        current.append(char)
    if current:
        lines.append("".join(current))
    return lines


def cjk_width(line, advance, ink):
    if not line:
        return 0
    return (len(line) - 1) * advance + ink


def wrap_english(text):
    lines = []
    rest = text
    while rest:
        take = 0
        last_space = 0
        while take < len(rest) and take < EN_CHARS:
            if rest[take] == " ":
                last_space = take
            take += 1
        if take < len(rest) and last_space:
            take = last_space
        lines.append(rest[:take])
        rest = rest[take:].lstrip()
    return lines


def main():
    failures = []
    for index, strings in enumerate(story_strings(), start=1):
        zh0, zh1, en0, en1 = strings
        large_budget = [
            math.ceil(len(zh0) / ZH_LARGE_CHARS),
            math.ceil(len(zh1) / ZH_LARGE_CHARS),
        ]
        small_budget = [
            math.ceil(len(zh0) / ZH_SMALL_CHARS),
            math.ceil(len(zh1) / ZH_SMALL_CHARS),
        ]
        large_lines = sum(large_budget)
        small_lines = sum(small_budget)
        if large_lines <= ZH_LARGE_MAX_LINES:
            layout = "24 px"
            budget = large_budget
            chars = ZH_LARGE_CHARS
            advance = ZH_LARGE_ADVANCE
            ink = ZH_LARGE_INK
            line_height = ZH_LINE_HEIGHT_LARGE
        elif small_lines <= ZH_SMALL_MAX_LINES:
            layout = "16 px"
            budget = small_budget
            chars = ZH_SMALL_CHARS
            advance = ZH_SMALL_ADVANCE
            ink = ZH_SMALL_INK
            line_height = ZH_LINE_HEIGHT_SMALL
        else:
            failures.append(
                f"floor {index}: Chinese needs {large_lines} lines at 24 px "
                f"and {small_lines} at 16 px; shorten it")
            layout = None

        if layout:
            actual = [wrap_cjk(zh0, chars), wrap_cjk(zh1, chars)]
            if [len(part) for part in actual] != budget:
                # Punctuation can reduce the real line count; it must never
                # exceed the budget the C renderer was given or it truncates.
                if any(len(actual[i]) > budget[i] for i in (0, 1)):
                    failures.append(
                        f"floor {index}: {layout} Chinese overflows its "
                        f"per-paragraph budget {budget}")
            total_lines = len(actual[0]) + len(actual[1])
            bottom = ZH_TEXT_Y + (total_lines - 1) * line_height + ink
            if bottom > ZH_BUTTON_Y:
                failures.append(
                    f"floor {index}: {layout} Chinese reaches y={bottom}, "
                    f"over the y={ZH_BUTTON_Y} button")
            for part in actual:
                for line in part:
                    width = cjk_width(line, advance, ink)
                    if width > TEXT_AREA_PX:
                        failures.append(
                            f"floor {index}: {layout} line {line!r} is {width}px, "
                            f"over the {TEXT_AREA_PX}px column")

        english = wrap_english(f"{en0} {en1}")
        if len(english) > EN_MAX_LINES:
            failures.append(
                f"floor {index}: English wraps to {len(english)} lines "
                f"(max {EN_MAX_LINES})")
        for line in english:
            if len(line) > EN_CHARS:
                failures.append(
                    f"floor {index}: English line {line!r} exceeds {EN_CHARS} chars")

    for index, name in enumerate(name_strings(), start=1):
        if len(name) > NAME_MAX_CHARS:
            failures.append(
                f"floor {index}: NPC name {name!r} exceeds {NAME_MAX_CHARS} chars")
        width = cjk_width(name, ZH_LARGE_ADVANCE, ZH_LARGE_INK)
        if width > NAME_MAX_PX:
            failures.append(
                f"floor {index}: NPC name {name!r} is {width}px, "
                f"over {NAME_MAX_PX}px")

    text = SOURCE.read_text(encoding="utf-8")
    for url in ("github.com/Wzdhehe/", "mosaico-magic-tower"):
        if url not in text:
            failures.append(f"settings URL fragment missing: {url}")
        if len(url) * 12 > URL_MAX_PX:
            failures.append(
                f"settings URL fragment {url!r} exceeds {URL_MAX_PX}px")

    if failures:
        for failure in failures:
            print("FAIL:", failure)
        return 1
    print("PASS: NPC names, Chinese/English dialogue and settings URL fit")
    return 0


if __name__ == "__main__":
    sys.exit(main())
