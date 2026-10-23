# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
import os
from pathlib import Path
import shutil
import tempfile
import unittest
from kymo_configurator.build_check import check_commands

CORE = Path(__file__).resolve().parents[3]


class CompilerAuditTests(unittest.TestCase):
    def test_actual_translation_units_and_mismatch(self):
        compiler = shutil.which('g++')
        fallback = Path.home() / '.platformio/packages/toolchain-gccmingw32/bin/g++.exe'
        compiler = compiler or (str(fallback) if fallback.exists() else None)
        if not compiler:
            self.skipTest('g++ required')
        old_path = os.environ['PATH']
        os.environ['PATH'] = str(Path(compiler).parent) + os.pathsep + old_path
        try:
            with tempfile.TemporaryDirectory(prefix='audit spaces ') as directory:
                source = Path(directory) / 'app.cpp'
                source.write_text('#include <kymo_config.h>\nint app() { return 0; }\n')
                entries = [{'directory': directory, 'file': str(path),
                            'arguments': [compiler, '-std=c++11', '-I' + str(CORE / 'src'),
                                          '-DKYMO_ENABLED=0', '-c', str(path), '-o', str(Path(directory) / 'out.o')]}
                           for path in (source, CORE / 'src/telemetry/kymo_protocol.cpp')]
                reports = check_commands(entries)
                self.assertEqual(reports[0]['values']['KYMO_ENABLED'], 0)
                entries[1]['arguments'][3] = '-DKYMO_ENABLED=1'
                with self.assertRaisesRegex(ValueError, 'Configuration mismatch'):
                    check_commands(entries)
        finally:
            os.environ['PATH'] = old_path
