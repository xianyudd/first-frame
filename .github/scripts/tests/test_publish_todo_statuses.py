"""Offline regression against the exact publisher used by grade.yml."""

import importlib.util
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

MODULE_PATH = Path(__file__).resolve().parents[1] / "publish_todo_statuses.py"
spec = importlib.util.spec_from_file_location("publisher", MODULE_PATH)
publisher = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = publisher
spec.loader.exec_module(publisher)
SHA = "a" * 40
RUN = "https://github.com/student/first-frame/actions/runs/123"


def report(lesson=4, checks=12):
    lines = ["TODO_PROTOCOL L4-v2"] if lesson == 4 else []
    lines += [f"TODO_STATUS {n} {checks} {checks}" for n in range(1, publisher.TASK_COUNTS[lesson] + 1)]
    if lesson == 4:
        lines += ["INTEGRATION_STATUS 3 3"]
    return "\n".join(lines) + "\n"


class Response:
    def __init__(self, status=201):
        self.status = status

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False


class PublisherTests(unittest.TestCase):
    def setUp(self):
        # Never let a missed test mock reach the network.
        guard = patch.object(publisher.urllib.request, "urlopen", side_effect=AssertionError("Unexpected live network call"))
        guard.start()
        self.addCleanup(guard.stop)

    def post(self, text, lesson=4, invalid=False):
        calls = []

        def opener(request, timeout):
            self.assertEqual(timeout, 20)
            self.assertEqual(request.get_method(), "POST")
            self.assertEqual(request.full_url, f"https://api.github.com/repos/student/first-frame/statuses/{SHA}")
            self.assertEqual(request.get_header("Authorization"), "Bearer mock-token")
            calls.append(json.loads(request.data))
            return Response()

        args = (text, lesson, "student/first-frame", SHA, RUN, "mock-token")
        if invalid:
            with self.assertRaisesRegex(RuntimeError, "invalid TODO report"):
                publisher.publish_report(*args, opener=opener)
        else:
            publisher.publish_report(*args, opener=opener)
        self.assertEqual(len(calls), publisher.TASK_COUNTS[lesson] + (lesson == 4))
        self.assertTrue(all(c["target_url"] == RUN for c in calls))
        return calls

    def test_thirteen_multi_digit_ids_and_no_aggregate_limit(self):
        parsed = publisher.parse_report(report(), 4)
        self.assertTrue(parsed.valid)
        self.assertGreater(sum(t for p, t in parsed.items.values()), 100)
        calls = self.post(report())
        self.assertEqual([c["context"] for c in calls],
                         [f"course/lesson-4/v2/todo-{n}" for n in range(1, 14)] +
                         ["course/lesson-4/v2/integration"])
        self.assertTrue(all(c["state"] == "success" for c in calls))

    def test_integration_failure_preserves_trustworthy_local_scores(self):
        calls = self.post(report().replace("INTEGRATION_STATUS 3 3", "INTEGRATION_STATUS 1 3"))
        self.assertTrue(all(c["state"] == "success" for c in calls[:-1]))
        self.assertEqual(calls[-1]["state"], "failure")
        self.assertEqual(calls[-1]["description"], "checks 1/3")

    def test_partial_predecessor_and_only_declared_blocks(self):
        text = report().replace("TODO_STATUS 8 12 12", "TODO_STATUS 8 11 12").replace(
            "TODO_STATUS 9 12 12", "TODO_BLOCKED 9 8\nTODO_STATUS 9 0 0").replace(
            "TODO_STATUS 10 12 12", "TODO_STATUS 10 1 12").replace(
            "TODO_STATUS 13 12 12", "TODO_BLOCKED 13 10\nTODO_STATUS 13 0 0")
        calls = self.post(text)
        self.assertEqual(calls[8]["description"], "blocked by task 8")
        self.assertEqual(calls[12]["description"], "blocked by task 10")
        self.assertEqual(calls[7]["state"], "failure")
        self.assertEqual(calls[9]["state"], "failure")

    def test_malformed_or_missing_report_publishes_only_unavailable(self):
        green = report()
        cases = {
            "missing artifact or compile failure": "",
            "compiler output": "error: compilation failed\nmake: Error 1\n",
            "missing item": green.replace("TODO_STATUS 13 12 12\n", ""),
            "duplicate item": green + "TODO_STATUS 10 12 12\n",
            "old five groups": report(1).replace("TODO_STATUS 4", "TODO_STATUS 5"),
            "missing header": green.replace("TODO_PROTOCOL L4-v2\n", ""),
            "old version": green.replace("L4-v2", "L4-v1"),
            "duplicate header": green + "TODO_PROTOCOL L4-v2\n",
            "malformed reserved line": green + "TODO_STATUS nope\n",
            "leading whitespace": green.replace("TODO_STATUS 10", " TODO_STATUS 10"),
            "negative": green.replace("TODO_STATUS 10 12 12", "TODO_STATUS 10 -1 12"),
            "passed over total": green.replace("TODO_STATUS 10 12 12", "TODO_STATUS 10 13 12"),
            "per-item bound": green.replace("TODO_STATUS 10 12 12", "TODO_STATUS 10 101 101"),
            "unbounded integer": green.replace("TODO_STATUS 10 12 12", "TODO_STATUS 10 9999999999 9999999999"),
            "illegal id": green + "TODO_STATUS 14 1 1\n",
            "zero id": green.replace("TODO_STATUS 10", "TODO_STATUS 0"),
            "unblocked zero": green.replace("TODO_STATUS 10 12 12", "TODO_STATUS 10 0 0"),
            "blocked without zero": green + "TODO_BLOCKED 9 8\n",
            "wrong prerequisite": green.replace("TODO_STATUS 9 12 12", "TODO_STATUS 9 0 0\nTODO_BLOCKED 9 10"),
            "green prerequisite": green.replace("TODO_STATUS 13 12 12", "TODO_STATUS 13 0 0\nTODO_BLOCKED 13 10"),
            "undeclared block": green.replace("TODO_STATUS 12 12 12", "TODO_STATUS 12 0 0\nTODO_BLOCKED 12 11"),
            "duplicate blocked": green + "TODO_BLOCKED 9 8\nTODO_BLOCKED 9 8\n",
            "missing integration": green.replace("INTEGRATION_STATUS 3 3\n", ""),
            "duplicate integration": green + "INTEGRATION_STATUS 3 3\n",
            "invalid integration": green.replace("INTEGRATION_STATUS 3 3", "INTEGRATION_STATUS 4 3"),
            "zero integration": green.replace("INTEGRATION_STATUS 3 3", "INTEGRATION_STATUS 0 0"),
            "malformed integration": green.replace("INTEGRATION_STATUS 3 3", "INTEGRATION_STATUS 3 3 trailing"),
        }
        for name, text in cases.items():
            with self.subTest(name=name):
                calls = self.post(text, invalid=True)
                self.assertTrue(all(c["state"] == "error" and c["description"] == "grading unavailable" for c in calls))

    def test_old_lessons_unchanged_context_counts_and_visual_semantics(self):
        for lesson in (1, 2, 3):
            with self.subTest(lesson=lesson):
                calls = self.post(report(lesson), lesson)
                self.assertEqual([c["context"] for c in calls],
                                 [f"course/lesson-{lesson}/todo-{n}" for n in range(1, publisher.TASK_COUNTS[lesson] + 1)])
                if lesson == 2:
                    self.assertEqual(calls[0]["description"], "source check 12/12")
                    self.assertEqual(calls[4]["description"], "source check 12/12")
        text = report(3).replace("TODO_STATUS 3 12 12", "TODO_STATUS 3 0 12").replace(
            "TODO_STATUS 4 12 12", "TODO_BLOCKED 4 3\nTODO_STATUS 4 0 0")
        self.assertEqual(self.post(text, 3)[3]["description"], "blocked by task 3")
        for lesson in (1, 2, 3):
            calls = self.post(report(lesson).replace("TODO_STATUS 1 12 12", "TODO_STATUS 1 0 0"), lesson, invalid=True)
            self.assertTrue(all(c["state"] == "error" for c in calls))

    def test_network_and_non_201_fail_loudly(self):
        args = (report(), 4, "student/first-frame", SHA, RUN, "mock-token")
        with self.assertRaisesRegex(RuntimeError, "503"):
            publisher.publish_report(*args, opener=lambda *a, **kw: Response(503))
        def offline(*args, **kwargs):
            raise OSError("offline")
        with self.assertRaisesRegex(OSError, "offline"):
            publisher.publish_report(*args, opener=offline)

    def test_cli_missing_artifact_is_unavailable_not_inherited(self):
        environment = dict(REF_NAME="my-lesson-4", REPOSITORY="student/first-frame",
                           COMMIT_SHA=SHA, RUN_URL=RUN, GH_TOKEN="mock-token")
        calls = []
        def opener(request, timeout):
            calls.append(json.loads(request.data))
            return Response()
        original = Path.cwd()
        with tempfile.TemporaryDirectory() as tmp, patch.dict(os.environ, environment), patch.object(publisher.urllib.request, "urlopen", opener):
            try:
                os.chdir(tmp)
                with self.assertRaisesRegex(RuntimeError, "invalid TODO report"):
                    publisher.main()
            finally:
                os.chdir(original)
        self.assertEqual(len(calls), 14)
        self.assertTrue(all(c["state"] == "error" for c in calls))

    def test_current_sha_and_repository_validation(self):
        for sha, repository in (("main", "student/first-frame"), (SHA, "../elsewhere")):
            with self.assertRaises(ValueError):
                publisher.publish_report(report(), 4, repository, sha, RUN, "mock-token")


if __name__ == "__main__":
    unittest.main()
