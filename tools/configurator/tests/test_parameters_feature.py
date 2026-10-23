# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""The parameter back channel is an optional feature: old projects stay valid."""
import unittest

from kymo_configurator.generation import _render
from kymo_configurator.model import default_project
from kymo_configurator.validation import validate


def codes(project):
    return {d.code for d in validate(project, "development")}


class ParametersFeatureTest(unittest.TestCase):
    def test_project_without_the_key_stays_valid(self):
        project = default_project()
        del project["profiles"]["development"]["features"]["parameters"]
        self.assertEqual(codes(project), codes(default_project()))

    def test_parameters_need_the_decoder_and_reach_the_header(self):
        project = default_project()
        features = project["profiles"]["development"]["features"]
        features["parameters"] = True
        self.assertIn("feature.parameters_decoder", codes(project))
        features["decoder"] = True
        self.assertNotIn("feature.parameters_decoder", codes(project))
        header = b"".join(content for name, content in _render(project, "development").items()
                          if name.endswith(".h"))
        self.assertIn(b"KYMO_ENABLE_PARAMETERS", header)
        self.assertIn(b"KYMO_ENABLE_DECODER", header)

    def test_wrong_type_is_reported(self):
        project = default_project()
        project["profiles"]["development"]["features"]["parameters"] = "yes"
        self.assertIn("boolean.type", codes(project))


if __name__ == "__main__":
    unittest.main()
