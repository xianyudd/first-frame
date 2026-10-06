#!/usr/bin/env python3
"""公开评测协议回归：仅验证调用方提供的二进制，不寻找或生成答案。"""
import argparse
import re
import subprocess
from pathlib import Path


def inspect(binary):
    runs = [subprocess.run([str(binary), *args], capture_output=True) for args in ([], ['--verbose'])]
    statuses = []
    for run in runs:
        assert run.stderr == b'', 'valid invocation wrote stderr'
        text = run.stdout.decode('ascii').replace('\r\n', '\n')
        assert run.returncode in (0, 1), f'unexpected exit {run.returncode}'
        rows = re.findall(r'^TODO_STATUS ([1-5]) (\d+) (\d+)$', text, re.M)
        assert [int(r[0]) for r in rows] == [1, 2, 3, 4, 5]
        assert 'TODO_BLOCKED' not in text and 'TODO_STATUS 6' not in text
        passed = sum(int(r[1]) for r in rows)
        total = sum(int(r[2]) for r in rows)
        assert 0 < total <= 100
        assert all(0 <= int(p) <= int(t) and int(t) > 0 for _, p, t in rows)
        assert f'Total: {passed}/{total} passed' in text
        assert run.returncode == (0 if passed == total else 1)
        failures = re.findall(r'^\[FAIL\].*$', text, re.M)
        assert len(failures) == total - passed
        assert all(re.search(r'test_todos\.cpp:\d+', f) and 'expected:' in f and 'actual: HP=' in f for f in failures)
        assert all('foregroundWidth=' in f for f in failures if f.startswith('[FAIL] 01 '))
        for name in ('Health bar', 'Contact feedback', 'Survival state', 'Kill experience', 'Upgrade choices'):
            assert name in text
        statuses.append((rows, failures, run.returncode))
    assert statuses[0] == statuses[1], 'default and verbose disagree'
    assert b'[PASS]' not in runs[0].stdout
    assert len(re.findall(rb'^\[PASS\]', runs[1].stdout, re.M)) == sum(int(r[1]) for r in statuses[1][0])
    invalid = subprocess.run([str(binary), '--unknown'], capture_output=True)
    assert invalid.returncode == 2
    return statuses[0][0]


def compile_failure(root, output):
    # false 取代编译器：必须失败，不能运行旧产物后假绿。
    run = subprocess.run(['make', '-C', str(root), 'test', 'CXX=false', f'TEST_OUT={output}'], capture_output=True)
    assert run.returncode != 0, 'compile failure was swallowed'


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('binary', type=Path)
    parser.add_argument('--compile-failure', type=Path, metavar='SCRATCH_OUTPUT')
    args = parser.parse_args()
    print('PROTOCOL PASS', inspect(args.binary.resolve()))
    if args.compile_failure:
        compile_failure(Path(__file__).resolve().parents[1], args.compile_failure.resolve())
        print('COMPILE FAILURE PROPAGATION PASS')
