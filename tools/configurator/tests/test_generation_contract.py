# SPDX-License-Identifier: MIT
"""Behavioral checks for publication, validation and damaged snapshot recovery."""
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from kymo_configurator.model import default_project
from kymo_configurator.generation import check, generate
from kymo_configurator.validation import validate


class GenerationContractTests(unittest.TestCase):
    def test_force_repairs_without_mutating_old_snapshot(self):
        project = default_project()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = generate(project, 'development', root)
            header = first / 'kymo_user_config.h'
            header.write_text('/* manual edit */\n')
            with self.assertRaises(ValueError):
                generate(project, 'development', root)
            repaired = generate(project, 'development', root, force=True)
            self.assertNotEqual(first, repaired)
            self.assertEqual(header.read_text(), '/* manual edit */\n')
            self.assertTrue(check(project, 'development', root)[0])
            self.assertEqual(generate(project, 'development', root), repaired)

    def test_interrupted_publication_preserves_previous_selection(self):
        project = default_project()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            generate(project, 'development', root)
            selected = (root / 'active_generation.json').read_bytes()
            changed = copy.deepcopy(project)
            changed['channels'][0]['period_ms'] = 40
            with patch('kymo_configurator.generation.os.replace', side_effect=OSError('simulated disk error')):
                with self.assertRaises(OSError):
                    generate(changed, 'development', root)
            self.assertEqual((root / 'active_generation.json').read_bytes(), selected)
            self.assertTrue(check(project, 'development', root)[0])

    def test_malformed_nested_objects_report_diagnostics(self):
        for field in ('profiles', 'bindings', 'port', 'build', 'resources', 'channels', 'symbols', 'contexts'):
            for bad in (None, 1, 'bad'):
                with self.subTest(field=field, bad=bad):
                    project = default_project()
                    project[field] = bad
                    self.assertTrue(any(d.severity == 'error' for d in validate(project, 'development')))

    def test_selector_cannot_escape_generation_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            generate(default_project(), 'development', root)
            (root / 'active_generation.json').write_text(json.dumps({'generation': '../outside'}))
            self.assertFalse(check(default_project(), 'development', root)[0])
