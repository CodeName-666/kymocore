# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
"""Application stylesheet: calm colours, clear hierarchy, readable help text."""

STYLE = """
QMainWindow, #sectionBody, QStackedWidget { background: #f7f8fa; }
#sidebar { background: #eef0f4; border-right: 1px solid #d9dde4; }
#sidebarTitle { font-size: 15px; font-weight: 600; color: #1f2430; padding: 6px 4px; }
#navigation { background: transparent; border: none; font-size: 13px; }
#navigation::item { padding: 8px 6px; border-radius: 6px; color: #1f2430; }
#navigation::item:selected { background: #dfe5f2; color: #1f2430; }
#sectionTitle { font-size: 20px; font-weight: 600; color: #1f2430; }
#sectionIntro { font-size: 13px; color: #3f4756; padding-bottom: 8px; }
#groupFrame { background: white; border: 1px solid #e0e3e9; border-radius: 8px; padding: 6px 10px; }
#groupTitle { font-size: 14px; font-weight: 600; color: #2a3140; padding-top: 4px; }
#fieldTitle { font-size: 13px; font-weight: 500; color: #1f2430; }
#fieldHelp { font-size: 12px; color: #5b6472; }
#fieldReason { font-size: 12px; color: #8a6d1f; font-style: italic; }
#fieldProblem { font-size: 12px; color: #b3261e; }
#targetBadge { font-family: monospace; font-size: 11px; color: #3b4a6b; background: #e8ecf5;
               border-radius: 4px; padding: 1px 6px; }
#presetCard { background: white; border: 1px solid #d9dde4; border-radius: 8px; padding: 4px; }
#presetTitle { font-size: 14px; font-weight: 600; }
#progress { color: #5b6472; padding: 10px 24px 0 24px; }
#primaryButton { background: #2f5bd3; color: white; border: none; border-radius: 6px;
                 padding: 7px 16px; font-weight: 600; }
#primaryButton:disabled { background: #a9b6d8; }
#modeCard { background: white; border: 1px solid #d9dde4; border-radius: 10px; padding: 18px; }
#modeTitle { font-size: 17px; font-weight: 600; color: #1f2430; }
#startTitle { font-size: 24px; font-weight: 600; color: #1f2430; }
#notice { background: #fff5d6; border: 1px solid #ecd58b; border-radius: 6px; padding: 8px; color: #5a4a12; }
#preview { font-family: monospace; font-size: 12px; }
"""
