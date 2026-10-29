# SPDX-License-Identifier: MIT
#!/usr/bin/env python3
"""Structural check: every C/C++ function has at most one final return.

Scans src/, examples/ and test/ (C, C++ and Arduino sketches). A function with a
return must have exactly one, as its last statement at function-body scope.
Heuristic tokenizer: comments, strings and preprocessor lines are ignored.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
SUFFIXES = {".c", ".cpp", ".h", ".ino"}
CONTROL = re.compile(r"\b(if|for|while|switch|do|else|catch)\s*\(?[^;{}]*$")
SIGNATURE = re.compile(
    r"\)\s*(const\s*)?(override\s*)?(noexcept\s*)?(:\s*[^;{}]*)?$", re.S)
RETURN = re.compile(r"\breturn\b")


def strip_code(text):
    """Remove comments, string/char literals and preprocessor lines."""
    text = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(\\.|[^"\\\n])*"', '""', text)
    text = re.sub(r"'(\\.|[^'\\\n])*'", "''", text)
    lines = []
    continued = False
    for line in text.split("\n"):
        directive = continued or line.lstrip().startswith("#")
        continued = directive and line.rstrip().endswith("\\")
        lines.append("" if directive else line)
    return "\n".join(lines)


def function_bodies(code):
    """Yield (line, body) for every function body found in code."""
    depth = 0
    index = 0
    start_of_statement = 0
    while index < len(code):
        char = code[index]
        if char in ";}":
            start_of_statement = index + 1
        if char == "{":
            head = code[start_of_statement:index].strip()
            if (SIGNATURE.search(head) and not CONTROL.search(head)
                    and "=" not in head.split("(")[0]):
                level = 1
                end = index + 1
                while end < len(code) and level > 0:
                    if code[end] == "{":
                        level += 1
                    elif code[end] == "}":
                        level -= 1
                    end += 1
                yield code.count("\n", 0, index) + 1, code[index + 1:end - 1]
                index = end
                start_of_statement = end
                continue
            depth += 1
            start_of_statement = index + 1
        elif char == "}":
            depth -= 1
        index += 1


def check_body(body):
    """Return an error message for an invalid return structure, else None."""
    returns = [m.start() for m in RETURN.finditer(body)]
    message = None
    if len(returns) > 1:
        message = f"{len(returns)} return statements"
    elif len(returns) == 1:
        position = returns[0]
        prefix = body[:position]
        depth = prefix.count("{") - prefix.count("}")
        tail = body[position:]
        semicolon = tail.find(";")
        if depth != 0 or tail[semicolon + 1:].strip():
            message = "return is not the last statement at function scope"
    return message


def main():
    failures = []
    for folder in ("src", "examples", "test"):
        for path in sorted((ROOT / folder).rglob("*")):
            if path.suffix not in SUFFIXES or ".pio" in path.parts:
                continue
            code = strip_code(path.read_text(encoding="utf-8"))
            for line, body in function_bodies(code):
                problem = check_body(body)
                if problem:
                    failures.append(f"{path.relative_to(ROOT)}:{line}: {problem}")
    for failure in failures:
        print(failure)
    if failures:
        sys.exit(1)
    print("single-return structure: PASS")


if __name__ == "__main__":
    main()
