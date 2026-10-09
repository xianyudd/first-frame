"""Parse public grader reports and publish commit statuses (no third-party deps)."""

import json
import os
import re
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

TASK_COUNTS = {1: 4, 2: 6, 3: 9, 4: 13}
PREREQUISITES = {3: {4: 3, 6: 2, 7: 6}, 4: {9: 8, 13: 10}}
MAX_CHECKS = 100  # Per item, not the sum of all items in the report.
NUMBER = r"[1-9][0-9]{0,2}"
COUNT = r"[0-9]{1,3}"


@dataclass
class Report:
    valid: bool = False
    items: dict = field(default_factory=dict)
    blocked: dict = field(default_factory=dict)
    integration: tuple | None = None


def parse_report(text, lesson):
    """Reject a whole malformed frame; a valid integration failure keeps scores."""
    ids = set(range(1, TASK_COUNTS[lesson] + 1))
    prerequisites = PREREQUISITES.get(lesson, {})
    report = Report(valid=True)
    headers = 0
    integration_lines = 0
    prefixes = ("TODO_STATUS", "TODO_BLOCKED")
    if lesson == 4:
        prefixes += ("TODO_PROTOCOL", "INTEGRATION_STATUS")
    for line in text.splitlines():
        if not line.lstrip().startswith(prefixes):
            continue
        match = re.fullmatch(rf"TODO_STATUS ({NUMBER}) ({COUNT}) ({COUNT})", line)
        block = re.fullmatch(rf"TODO_BLOCKED ({NUMBER}) ({NUMBER})", line)
        integration = re.fullmatch(rf"INTEGRATION_STATUS ({COUNT}) ({COUNT})", line)
        if match:
            number, passed, total = map(int, match.groups())
            if number not in ids or number in report.items or not 0 <= passed <= total <= MAX_CHECKS:
                report.valid = False
            report.items[number] = (passed, total)
        elif block:
            number, prior = map(int, block.groups())
            if number in report.blocked or prerequisites.get(number) != prior:
                report.valid = False
            report.blocked[number] = prior
        elif lesson == 4 and line == "TODO_PROTOCOL L4-v2":
            headers += 1
        elif lesson == 4 and integration:
            integration_lines += 1
            passed, total = map(int, integration.groups())
            if not 0 <= passed <= total <= MAX_CHECKS or total == 0:
                report.valid = False
            report.integration = (passed, total)
        else:
            report.valid = False
    if set(report.items) != ids or (lesson == 4 and (headers != 1 or integration_lines != 1)):
        report.valid = False
    for number, (_, total) in report.items.items():
        if total == 0 and number not in report.blocked:
            report.valid = False
    for number, prior in report.blocked.items():
        prior_value = report.items.get(prior)
        if report.items.get(number) != (0, 0) or prior_value is None or (
            prior_value[1] > 0 and prior_value[0] == prior_value[1]
        ):
            report.valid = False
    return report


def status_payloads(report, lesson, run_url):
    """Use versioned contexts only for L4; all earlier contexts stay unchanged."""
    prefix = f"course/lesson-{lesson}" + ("/v2" if lesson == 4 else "")
    payloads = []
    for number in range(1, TASK_COUNTS[lesson] + 1):
        if not report.valid:
            state, description = "error", "grading unavailable"
        elif number in report.blocked:
            state, description = "error", f"blocked by task {report.blocked[number]}"
        else:
            passed, total = report.items[number]
            state = "success" if passed == total else "failure"
            kind = "source check" if lesson == 2 and number in (1, 5) else "checks"
            description = f"{kind} {passed}/{total}"
        payloads.append(dict(state=state, context=f"{prefix}/todo-{number}",
                             description=description, target_url=run_url))
    if lesson == 4:
        state, description = "error", "grading unavailable"
        if report.valid:
            passed, total = report.integration
            state = "success" if passed == total else "failure"
            description = f"checks {passed}/{total}"
        payloads.append(dict(state=state, context=f"{prefix}/integration",
                             description=description, target_url=run_url))
    return payloads


def publish_report(text, lesson, repository, sha, run_url, token, opener=None):
    """Publish every expected status, even when the artifact is missing/invalid."""
    if not re.fullmatch(r"[0-9a-fA-F]{40}", sha):
        raise ValueError("Invalid current commit SHA")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*/[A-Za-z0-9][A-Za-z0-9_.-]*", repository):
        raise ValueError("Invalid repository")
    report = parse_report(text, lesson)
    endpoint = f"https://api.github.com/repos/{repository}/statuses/{sha}"
    opener = opener or urllib.request.urlopen
    for payload in status_payloads(report, lesson, run_url):
        request = urllib.request.Request(endpoint, data=json.dumps(payload).encode(), method="POST", headers={
            "Authorization": "Bearer " + token,
            "Accept": "application/vnd.github+json",
            "Content-Type": "application/json",
            "X-GitHub-Api-Version": "2022-11-28",
        })
        with opener(request, timeout=20) as response:
            if response.status != 201:
                raise RuntimeError(f"Commit status failed: {response.status}")
    if not report.valid:
        raise RuntimeError("Incomplete or invalid TODO report; no passing statuses published")
    return report


def main():
    match = re.fullmatch(r"my-lesson-([1-4])", os.environ["REF_NAME"])
    if not match:
        raise ValueError("Unsupported practice branch")
    path = Path("results/grade_output.txt")
    text = path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""
    publish_report(text, int(match[1]), os.environ["REPOSITORY"], os.environ["COMMIT_SHA"],
                   os.environ["RUN_URL"], os.environ["GH_TOKEN"])


if __name__ == "__main__":
    main()
