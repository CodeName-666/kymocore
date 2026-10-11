# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Small real Qt Widgets workflow checks; validation remains in the shared engine."""
import os
import unittest

try:
    from PySide6.QtWidgets import QApplication
    from kymo_configurator.gui.main_window import MainWindow
except ImportError:
    QApplication=None
    MainWindow=None

@unittest.skipIf(QApplication is None,"PySide6 GUI extra is not installed")
class GuiWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.environ.setdefault("QT_QPA_PLATFORM","offscreen")
        cls.app=QApplication.instance() or QApplication([])
    def test_profile_values_survive_off_switch_and_channel_table_edits(self):
        window=MainWindow()
        window.profile.setCurrentText("production")
        window.enabled.setChecked(True)
        window.feature_checks["x"][0].setChecked(True)
        window.enabled.setChecked(False)
        self.assertFalse(window.feature_checks["x"][0].isEnabled())
        self.assertTrue(window.active()["features"]["x"])
        window.profile.setCurrentText("development")
        window.profile.setCurrentText("production")
        self.assertTrue(window.active()["features"]["x"])
        self.assertFalse(window.active()["enabled"])
        window.enabled.setChecked(True)
        model=window.channel_view.model()
        model.setData(model.index(0,1),"Edited channel")
        self.assertEqual(window.doc["channels"][0]["name"],"Edited channel")
        window.dirty=False
        window.close()

    def test_trace_binding_clear_and_port_verification_preservation(self):
        window=MainWindow()
        window.doc['port']['capabilities_verified']=True
        window.active()['trace']['bindings']['transport_busy']='port_busy'
        window.profile_changed(window.profile.currentText())
        self.assertTrue(window.doc['port']['capabilities_verified'])
        window.trace_binding_inputs['transport_busy'].clear()
        window._pull_ui()
        self.assertNotIn('transport_busy',window.active()['trace']['bindings'])
        window.port.setCurrentText('esp32_freertos')
        self.assertFalse(window.doc['port']['capabilities_verified'])
        window.dirty=False
        window.close()

if __name__=="__main__": unittest.main()
