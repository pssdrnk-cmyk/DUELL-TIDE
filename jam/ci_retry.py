#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Re-run the jobs of a finished "Host tests" run that GitHub itself failed (run by .github/workflows/ci-retry.yml; test: jam/test_ci_retry.py).

  jam/ci_retry.py RUN_ID [--dry-run] [--force]      (needs GITHUB_TOKEN and GITHUB_REPOSITORY; Actions write permission to re-run)

GitHub's hosted runners sometimes never start a job ("The job was not acquired by Runner of type hosted even after multiple attempts": no step ran,
runner id 0) or kill one in the middle ("The runner has received a shutdown signal", "lost communication with the server"). Those need a click on
"Re-run failed jobs" and nothing else. This script does that click, once or twice, and only for that kind of failure:

  - the run must have finished red or cancelled, and be below attempt 3 (so at most two automatic re-runs);
  - every red or cancelled job must be a runner failure: no step ran at all, or the job's annotations say the runner died. If even one job failed
    for another reason (a test, a build, a timeout, a user's cancel) nothing is re-run: the run is red for a real reason and a human looks at it;
  - a run that a newer run replaced is left alone. Host tests has `cancel-in-progress`: the older run ends "cancelled" with the annotation
    "Canceling since a higher priority waiting request ..."; re-running it would cancel the newer run in turn. So does a run whose commit is no
    longer the head of its branch or tag, and a run for which a newer run of the same workflow and commit exists (--force skips only the head
    test, for a manual proof on an old run);
  - one request re-runs the failed jobs of the run ("Re-run failed jobs"; the jobs that passed are kept). Since every red job was a runner failure,
    that is exactly the runner-failed set. (A job-by-job request does not work: the first one puts the whole run back to running, the rest get 403.)
The decision and the reason go to the job summary and to stdout. The token is only read from the environment and never printed."""
import json
import os
import sys
import urllib.error
import urllib.request

MAX_ATTEMPT = 3                       # a run at attempt 3 is not re-run again: attempt 1 + two automatic re-runs
BAD = ("failure", "cancelled", "timed_out")
# annotation texts that mean the runner (not our job) failed
INFRA = ("not acquired by runner", "runner has received a shutdown signal", "lost communication with the server",
         "the hosted runner encountered an error", "the runner has been shutdown", "runner application unexpectedly",
         "internal server error", "failed to download action")
SUPERSEDED = "higher priority waiting request"


def classify(job, notes):
    """-> 'infra' (the runner failed: re-run), 'superseded' (a newer run replaced it) or 'real' (anything else)"""
    text = " ".join(notes).lower()
    if SUPERSEDED in text:
        return "superseded"
    if any(k in text for k in INFRA):
        return "infra"
    if not job.get("steps") and job.get("conclusion") in BAD:        # nothing ran: no runner picked it up
        return "infra"
    return "real"


def decide(run, jobs, notes_of, newer_run, head_sha, force=False):
    """-> (job ids to re-run, reason). notes_of(job) -> its annotation messages; newer_run: a newer run of the same workflow and commit exists;
    head_sha: the commit the run's branch or tag points at now (None = unknown)"""
    if run["status"] != "completed" or run["conclusion"] not in ("failure", "cancelled"):
        return [], f"run {run['id']} is {run['status']} / {run['conclusion']}: nothing to re-run"
    if run.get("run_attempt", 1) >= MAX_ATTEMPT:
        return [], f"run {run['id']} is at attempt {run.get('run_attempt')}: the automatic re-runs are used up"
    if newer_run:
        return [], f"a newer run of this workflow exists for commit {run['head_sha'][:8]}: this one was replaced"
    if not force and head_sha != run["head_sha"]:
        return [], f"commit {run['head_sha'][:8]} is no longer the head of {run.get('head_branch')} ({(head_sha or 'unknown')[:8]}): the run is stale"
    kinds = {}
    for j in jobs:
        if j["conclusion"] in BAD:
            kinds[j["id"]] = (classify(j, notes_of(j)), j["name"])
    if not kinds:
        return [], "no red or cancelled job in the latest attempt"
    real = [n for k, n in kinds.values() if k == "real"]
    sup = [n for k, n in kinds.values() if k == "superseded"]
    if real:
        return [], f"{len(real)} job(s) failed for a real reason, not re-run: {', '.join(real[:4])}"
    if sup:
        return [], f"{len(sup)} job(s) were cancelled by a newer request: not re-run"
    return sorted(kinds), f"{len(kinds)} job(s) the runner failed: {', '.join(n for _, n in kinds.values())}"


class Hub:
    def __init__(self, repo, token):
        self.repo, self.token = repo, token

    def call(self, method, path, body=None):
        req = urllib.request.Request("https://api.github.com/repos/" + self.repo + path, method=method,
                                     data=json.dumps(body).encode() if body is not None else None,
                                     headers={"Authorization": "Bearer " + self.token, "Accept": "application/vnd.github+json",
                                              "X-GitHub-Api-Version": "2022-11-28", "User-Agent": "duell-ci-retry"})
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                data = r.read()
                return r.status, (json.loads(data) if data else {})
        except urllib.error.HTTPError as e:
            data = e.read()
            try:
                return e.code, json.loads(data)
            except ValueError:
                return e.code, {"message": data.decode(errors="replace")[:300]}


def run_once(hub, run_id, dry=False, force=False, say=print):
    """-> (decision text, [ids of the jobs re-run: empty when nothing was or could be])"""
    st, run = hub.call("GET", f"/actions/runs/{run_id}")
    if st != 200:
        return f"run {run_id} not readable ({st}): {run.get('message')}", []
    st, d = hub.call("GET", f"/actions/runs/{run_id}/jobs?filter=latest&per_page=100")
    if st != 200:
        return f"jobs of run {run_id} not readable ({st})", []
    jobs = d["jobs"]
    st, d = hub.call("GET", f"/actions/workflows/{run['workflow_id']}/runs?head_sha={run['head_sha']}&per_page=30")
    newer = st == 200 and any(r["id"] > run["id"] for r in d.get("workflow_runs", []))
    ref = run.get("head_branch")
    st, c = hub.call("GET", "/commits/" + ref) if ref else (0, {})       # a branch or a tag name: the commit it points at now
    head = c.get("sha") if st == 200 and isinstance(c, dict) else None

    def notes_of(job):
        s, a = hub.call("GET", f"/check-runs/{job['id']}/annotations?per_page=50")
        return [x.get("message") or "" for x in a] if s == 200 and isinstance(a, list) else []

    ids, why = decide(run, jobs, notes_of, newer, head, force)
    say(f"ci-retry: run {run_id} attempt {run.get('run_attempt')}: {why}")
    if not ids:
        return why, []
    if dry:
        say(f"ci-retry: dry run, would re-run the failed jobs of run {run_id}: {ids}")
        return why, []
    st, r = hub.call("POST", f"/actions/runs/{run_id}/rerun-failed-jobs", {"enable_debug_logging": False})
    say(f"ci-retry: re-run of the failed jobs of run {run_id}: {st}" + ("" if st in (201, 204) else f" {r.get('message')}"))
    return why, ids if st in (201, 204) else []


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    if len(args) != 1 or not args[0].isdigit():
        sys.exit(__doc__.split("\n\n")[0])
    repo, token = os.environ.get("GITHUB_REPOSITORY"), os.environ.get("GITHUB_TOKEN")
    if not repo or not token:
        sys.exit("ci-retry: GITHUB_REPOSITORY and GITHUB_TOKEN are needed")
    lines = []

    def say(t):
        print(t)
        lines.append(t)
    why, done = run_once(Hub(repo, token), int(args[0]), "--dry-run" in argv, "--force" in argv, say)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a") as f:
            f.write("### Host tests retry\n" + "\n".join("- " + x for x in lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
