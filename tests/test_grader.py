#!/usr/bin/env python3
"""Public protocol checks for a caller-provided binary; never load solutions."""
import argparse
import importlib.util
import re
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True


def load_parser(path):
    spec = importlib.util.spec_from_file_location('l4_public_parser', path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def inspect(binary, parser_module=None):
    runs = [subprocess.run([str(binary), *args], capture_output=True) for args in ([], ['--verbose'])]
    statuses = []
    for run in runs:
        assert run.stderr == b'', 'valid invocation wrote stderr'
        text = run.stdout.decode('ascii').replace('\r\n', '\n')
        assert run.returncode in (0, 1), f'unexpected exit {run.returncode}'
        assert re.findall(r'^TODO_PROTOCOL.*$', text, re.M) == ['TODO_PROTOCOL L4-v2']
        rows = re.findall(r'^TODO_STATUS ([1-9]\d*) (\d+) (\d+)$', text, re.M)
        assert [int(r[0]) for r in rows] == list(range(1, 14))
        assert len(re.findall(r'^TODO_STATUS.*$', text, re.M)) == 13
        blocked = re.findall(r'^TODO_BLOCKED ([1-9]\d*) ([1-9]\d*)$', text, re.M)
        assert len(blocked) == len(re.findall(r'^TODO_BLOCKED.*$', text, re.M))
        assert len(set(blocked)) == len(blocked)
        counts = {int(i): (int(p), int(t)) for i, p, t in rows}
        dependencies = {int(i): int(d) for i, d in blocked}
        for i, d in dependencies.items():
            assert (i, d) in ((9, 8), (13, 10)), 'undeclared dependency'
            assert counts[i] == (0, 0), 'blocked item must be unexecuted'
            assert 0 <= counts[d][0] < counts[d][1], 'passing prerequisite cannot block'
        for i, (p, t) in counts.items():
            assert 0 <= p <= t <= 100
            assert t > 0 or i in dependencies, 'zero checks cannot pass'
        integration = re.findall(r'^INTEGRATION_STATUS (\d+) (\d+)$', text, re.M)
        assert len(integration) == len(re.findall(r'^INTEGRATION_STATUS.*$', text, re.M)) == 1
        ip, it = map(int, integration[0])
        assert 0 <= ip <= it <= 100 and it > 0
        passed = sum(p for p, _ in counts.values()) + ip
        total = sum(t for _, t in counts.values()) + it
        assert f'Total: {passed}/{total} passed' in text
        assert run.returncode == (0 if not blocked and passed == total else 1)
        failures = re.findall(r'^\[FAIL\].*$', text, re.M)
        assert len(failures) == total - passed
        assert all(re.search(r'test_todos\.cpp:\d+', f) and 'expected:' in f and 'actual: HP=' in f for f in failures)
        assert all('foregroundWidth=' in f for f in failures if f.startswith('[FAIL] 01 '))
        for label in ('L4-01', 'L4-02-A', 'L4-02-B', 'L4-02-C', 'L4-03-A', 'L4-03-B',
                      'L4-04-A', 'L4-04-B', 'L4-05-A', 'L4-05-B', 'L4-05-C', 'L4-05-D', 'L4-06'):
            assert label in text
        if parser_module:
            report = parser_module.parse_report(text, 4)
            assert report.valid, 'public CI parser rejected actual backend output'
            assert report.items == counts and report.blocked == dependencies
            assert report.integration == (ip, it)
        measurements = re.findall(r'^\[EXPERIMENT\].*$', text, re.M)
        statuses.append((rows, blocked, integration, failures, run.returncode, measurements))
    assert statuses[0] == statuses[1], 'default and verbose disagree'
    assert b'[PASS]' not in runs[0].stdout
    assert len(re.findall(rb'^\[PASS\]', runs[1].stdout, re.M)) == sum(int(r[1]) for r in statuses[1][0]) + int(statuses[1][2][0][0])
    invalid = subprocess.run([str(binary), '--unknown'], capture_output=True)
    assert invalid.returncode == 2
    return statuses[0][0]


def compile_failure(root, output):
    # A failing compiler must not execute an already-existing output file.
    output.parent.mkdir(parents=True, exist_ok=True)
    original = output.read_bytes() if output.exists() else None
    output.write_text('#!/bin/sh\nprintf "STALE_BINARY_WAS_RUN\\n"\nexit 0\n', encoding='ascii')
    output.chmod(0o755)
    try:
        run = subprocess.run(['make', '-C', str(root), 'test', 'CXX=false', f'TEST_OUT={output}'], capture_output=True)
        assert run.returncode != 0, 'compile failure was swallowed'
        assert b'STALE_BINARY_WAS_RUN' not in run.stdout, 'stale binary ran after failed compilation'
    finally:
        if original is None:
            output.unlink()
        else:
            output.write_bytes(original)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('binary', type=Path)
    parser.add_argument('--parser', type=Path, help='optional path to actual public CI parser')
    parser.add_argument('--compile-failure', type=Path, metavar='SCRATCH_OUTPUT')
    args = parser.parse_args()
    module = load_parser(args.parser.resolve()) if args.parser else None
    print('PROTOCOL PASS', inspect(args.binary.resolve(), module))
    if args.compile_failure:
        compile_failure(Path(__file__).resolve().parents[1], args.compile_failure.resolve())
        print('COMPILE FAILURE PROPAGATION PASS')
