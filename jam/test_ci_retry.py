#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Self-test of jam/ci_retry.py on made-up GitHub answers (no network): only a runner failure is re-run, never a real one, a replaced run or a spent one."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ci_retry  # noqa: E402

SHA = "a" * 40
bad = 0


def expect(what, ok):
    global bad
    if not ok:
        bad += 1
        print("ci_retry: FAIL", what)


def job(jid, name, conclusion, steps=15):
    return {"id": jid, "name": name, "conclusion": conclusion, "steps": [{"name": "s"}] * steps}


class Fake:
    """a GitHub that answers from tables and records the re-run requests"""
    def __init__(self, run, jobs, notes, newer=(), head=SHA):
        self.run, self.jobs, self.notes, self.newer, self.head, self.posted = run, jobs, notes, newer, head, []

    def call(self, method, path, body=None):
        if method == "POST":
            self.posted.append(path)
            return 201, {}
        if "/annotations" in path:
            return 200, [{"message": m} for m in self.notes.get(int(path.split("/check-runs/")[1].split("/")[0]), [])]
        if "/jobs?" in path:
            return 200, {"jobs": self.jobs}
        if "/workflows/" in path:
            return 200, {"workflow_runs": [{"id": self.run["id"]}] + [{"id": i} for i in self.newer]}
        if "/commits/" in path:
            return 200, {"sha": self.head}
        return 200, self.run


def go(jobs, notes=None, attempt=1, conclusion="failure", **kw):
    run = {"id": 100, "status": "completed", "conclusion": conclusion, "run_attempt": attempt, "head_sha": SHA, "head_branch": "jam11", "workflow_id": 7}
    hub = Fake(run, jobs, notes or {}, **kw)
    why, done = ci_retry.run_once(hub, 100, say=lambda t: None, force=kw.get("force", False)) if "force" not in kw else (None, None)
    return why, done, hub.posted


ACQ = {2: ["The job was not acquired by Runner of type hosted even after multiple attempts"]}
ok1, ok2 = job(1, "host-tests (0)", "success"), job(3, "host-tests (2)", "success")

why, done, posted = go([ok1, job(2, "host-tests (1)", "cancelled", steps=0), ok2], ACQ)
expect("a job no runner picked up is re-run, by one request for the failed jobs of the run", done == [2] and posted == ["/actions/runs/100/rerun-failed-jobs"])

why, done, posted = go([ok1, job(2, "host-tests (1)", "cancelled", steps=0), job(4, "host-tests (3)", "cancelled", steps=0)], {})
expect("a job with no step at all is a runner failure even without a note; two of them, still one request", done == [2, 4] and len(posted) == 1)

why, done, posted = go([ok1, job(2, "host-tests (1)", "failure")], {2: ["The runner has received a shutdown signal. This can happen when the runner service is stopped."]})
expect("a shutdown signal in the middle of a job is a runner failure", done == [2])

why, done, posted = go([ok1, job(2, "host-tests (1)", "failure")], {2: ["Process completed with exit code 1."]})
expect("a failed test step is never re-run", done == [] and posted == [] and "real reason" in why)

why, done, posted = go([job(2, "host-tests (1)", "cancelled", steps=0), job(4, "host-tests (3)", "failure")], {2: ACQ[2], 4: ["Process completed with exit code 1."]})
expect("one real failure next to a runner failure: nothing is re-run", done == [] and posted == [])

why, done, posted = go([job(2, "host-tests (1)", "cancelled")], {2: ["Canceling since a higher priority waiting request for host-tests-refs/heads/jam11 exists", "The operation was canceled."]}, conclusion="cancelled")
expect("a run replaced by a newer request (concurrency) is not re-run", done == [] and posted == [])

why, done, posted = go([job(2, "host-tests (1)", "cancelled")], {2: ["The operation was canceled."]}, conclusion="cancelled")
expect("a plain cancel (a person, a timeout) is not re-run", done == [] and posted == [])

why, done, posted = go([job(2, "host-tests (1)", "cancelled", steps=0)], ACQ, newer=[101])
expect("a newer run of the same commit exists: not re-run", done == [] and posted == [] and "newer run" in why)

why, done, posted = go([job(2, "host-tests (1)", "cancelled", steps=0)], ACQ, head="b" * 40)
expect("the branch moved on: the stale run is not re-run", done == [] and "no longer the head" in why)

why, done, posted = go([job(2, "host-tests (1)", "cancelled", steps=0)], ACQ, attempt=3)
expect("attempt 3 is the last: no third automatic re-run", done == [] and "used up" in why)
why, done, posted = go([job(2, "host-tests (1)", "cancelled", steps=0)], ACQ, attempt=2)
expect("attempt 2 still gets its one re-run", done == [2])

run = {"id": 100, "status": "completed", "conclusion": "success", "run_attempt": 1, "head_sha": SHA, "head_branch": "jam11", "workflow_id": 7}
why, done = ci_retry.run_once(Fake(run, [ok1], {}), 100, say=lambda t: None)
expect("a green run is left alone", done == [])

run = {"id": 100, "status": "completed", "conclusion": "failure", "run_attempt": 1, "head_sha": SHA, "head_branch": "old", "workflow_id": 7}
hub = Fake(run, [job(2, "host-tests (1)", "cancelled", steps=0)], ACQ, head="c" * 40)
why, done = ci_retry.run_once(hub, 100, force=True, say=lambda t: None)
expect("--force skips only the head test (a manual proof on an old run)", done == [2])
hub = Fake(run, [job(2, "host-tests (1)", "cancelled", steps=0)], ACQ, head="c" * 40)
why, done = ci_retry.run_once(hub, 100, dry=True, force=True, say=lambda t: None)
expect("--dry-run posts nothing", done == [] and hub.posted == [])

print(f"ci_retry: {'FAIL' if bad else 'ok'} ({bad} failed)")
sys.exit(1 if bad else 0)
