# SPDX-License-Identifier: MIT
"""Read-only GCC/Clang configuration audit of compile_commands argument arrays.

This reproduces preprocessing, not the provenance of an existing object. Always
perform a clean integration build after a successful audit.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import subprocess
import tempfile

FEATURES = ('RUNTIME', 'X', 'Z', 'TIMESTAMP', 'CRC', 'DECODER', 'CPP',
            'TRACE', 'TRACE_ISR', 'TRACE_SCHEDULER', 'TRACE_FLOW')
MACROS = ('KYMO_ENABLED',) + tuple('KYMO_ENABLE_' + key for key in FEATURES) + tuple(
    'KYMO_EFFECTIVE_' + key for key in FEATURES)


def inspect_entry(entry: dict) -> dict:
    """Evaluate requested/effective booleans after the actual source's includes."""
    directory = Path(entry['directory']).resolve()
    source = Path(entry['file'])
    source = source.resolve() if source.is_absolute() else (directory / source).resolve()
    arguments = entry.get('arguments')
    if not isinstance(arguments, list) or not arguments or not all(isinstance(x, str) for x in arguments):
        raise ValueError('Compile database entries must provide an arguments array; shell command strings are unsupported')
    filtered = [arguments[0]]
    skip = False
    for arg in arguments[1:]:
        if skip:
            skip = False
            continue
        if arg in ('-o', '-MF', '-MT', '-MQ'):
            skip = True
            continue
        if arg in ('-c', '-MD', '-MMD', '-MP', '-M', '-MM'):
            continue
        if arg.startswith(('-o', '-MF', '-MT', '-MQ')):
            continue
        path = Path(arg)
        if not arg.startswith('-') and (directory / path).resolve() == source:
            continue
        filtered.append(arg)
    with tempfile.TemporaryDirectory(prefix='kymo-config-audit-') as temp:
        probe = Path(temp) / ('audit.c' if source.suffix == '.c' else 'audit.cpp')
        lines = [f'#include "{source.as_posix()}"', '#include "kymo_config.h"']
        for name in MACROS:
            lines.extend((f'#if {name}', f'KYMO_AUDIT "{name}" 1', '#else', f'KYMO_AUDIT "{name}" 0', '#endif'))
        probe.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        result = subprocess.run([*filtered, '-E', '-P', str(probe)], cwd=directory,
                                capture_output=True, text=True, timeout=120)
        if result.returncode:
            raise ValueError(f'{source}: preprocessing failed\n{result.stderr}')
        values = {}
        for line in result.stdout.splitlines():
            fields = line.split()
            if len(fields) == 3 and fields[0] == 'KYMO_AUDIT' and fields[1].strip('"') in MACROS:
                values[fields[1].strip('"')] = int(fields[2])
        if len(values) != len(MACROS):
            raise ValueError(f'{source}: incomplete macro audit')
        return {'file': str(source), 'values': values}


def check_commands(entries: list[dict]) -> list[dict]:
    """Raise on differences; return the compiler-resolved settings for every TU."""
    if not entries:
        raise ValueError('No translation units selected')
    reports = [inspect_entry(entry) for entry in entries]
    reference = reports[0]
    for report in reports[1:]:
        differences = [name for name in MACROS if report['values'][name] != reference['values'][name]]
        if differences:
            raise ValueError(f"Configuration mismatch: {reference['file']} versus {report['file']}: " + ', '.join(differences))
    return reports


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compile_commands', type=Path)
    args = parser.parse_args(argv)
    try:
        reports = check_commands(json.loads(args.compile_commands.read_text(encoding='utf-8')))
    except (ValueError, KeyError, OSError, subprocess.TimeoutExpired) as exc:
        parser.exit(1, str(exc) + '\n')
    print(json.dumps({'translation_units': reports,
                      'evidence': 'preprocessor audit; existing object provenance is not verified'}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
