"""Publication guard: never silently export an obsolete Ideas schema."""
import importlib.util
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('package_firmware',ROOT/'tools/package_firmware.py')
package=importlib.util.module_from_spec(spec);spec.loader.exec_module(package)

class IdeasSchemaTests(unittest.TestCase):
    def test_current_export(self):
        package.require_schema('esp-iris-system-update/0.2')

    def test_legacy_is_rejected_for_website(self):
        with self.assertRaisesRegex(ValueError,'ABI 2'):
            package.require_schema('esp-iris-system-update/v1')

    def test_unknown_or_missing_is_rejected(self):
        for value in (None,'','esp-iris-system-update/v2'):
            with self.subTest(schema=value),self.assertRaises(ValueError):
                package.require_schema(value)

    def test_explicit_legacy_local_export(self):
        package.require_schema('esp-iris-system-update/v1','esp-iris-system-update/v1')

if __name__=='__main__':unittest.main()
