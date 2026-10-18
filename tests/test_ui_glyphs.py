"""All authored UI text must have glyphs in the generated bitmap subset."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UIGlyphs(unittest.TestCase):
    def test_authored_text_is_renderable(self):
        source = (ROOT / "main/game_view.c").read_text(encoding="utf-8")
        strings = re.findall(r'"(?:\\.|[^"\\])*"', source)
        required = {ord(ch) for value in strings for ch in value if ord(ch) > 127}
        header = (ROOT / "main/ui_font.h").read_text(encoding="utf-8")
        glyphs = [int(cp, 16) for cp in re.findall(r'\{0x([0-9a-fA-F]+),\s*\{', header)]
        self.assertTrue(required)
        self.assertEqual(glyphs, sorted(set(glyphs)), "binary lookup needs sorted unique glyphs")
        missing = required - set(glyphs)
        self.assertFalse(missing, "Missing UI glyphs: " + "".join(map(chr, sorted(missing))))


if __name__ == "__main__":
    unittest.main()
