#!/usr/bin/env python3
"""rootexec.py -- capability-gated root execution harness.

WHY THIS EXISTS
---------------
Root is ONLY reachable through this harness. A model/process that can invoke
rootexec is constrained to the command allowlist in the policy file -- it
cannot invent arbitrary `sudo` calls. This blocks "dumb" models (weak or
untrusted automation) from escalating to root outside the vetted set.

The sudo password is PIPED via `sudo -S` (from ROOTEXEC_PW env or --pw), never
typed interactively. That is the mechanism: root via python with the password
fed on stdin. A weak model without the password simply cannot escalate -- the
harness refuses to prompt.

This is the sanctioned privileged channel. When a build step needs root
(e.g. `apt-get install -y ccache`), it goes through here -- not a bare
`sudo` from the agent. Everything else is refused.

USAGE
-----
  rootexec.py [--policy FILE] [--dry-run] [--json] -- <command...>
  rootexec.py --list            # print the allowed-command specs
  rootexec.py --self-test       # demonstrate allow + block paths

POLICY (JSON)
-------------
A list of objects: { "match": <regex over the joined argv>, "desc": <str> }.
A command is PERMITTED iff it matches at least one entry. Otherwise BLOCKED.
Match is anchored (full-string) against " ".join(argv).

EXIT CODES
----------
  0  command ran and exited 0
  N  command ran and exited N (N != 0)
  2  BLOCKED -- command not in allowlist (the dumb-model guard)
  3  usage / harness error
"""
import argparse
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone

DEFAULT_POLICY = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "rootexec.policy.json")
AUDIT_LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                         ".rootexec_audit.log")


def audit(decision, cmd, detail=""):
    try:
        ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
        euid = os.geteuid()
        line = f"{ts} euid={euid} decision={decision} cmd={cmd!r} {detail}\n"
        with open(AUDIT_LOG, "a") as f:
            f.write(line)
    except Exception:
        pass  # never let audit logging break the harness


def load_policy(path):
    try:
        with open(path) as f:
            data = json.load(f)
    except FileNotFoundError:
        sys.stderr.write(f"rootexec: policy not found: {path}\n")
        sys.exit(3)
    except json.JSONDecodeError as e:
        sys.stderr.write(f"rootexec: bad policy JSON: {e}\n")
        sys.exit(3)
    if not isinstance(data, list):
        sys.stderr.write("rootexec: policy must be a JSON list\n")
        sys.exit(3)
    specs = []
    for i, entry in enumerate(data):
        if not isinstance(entry, dict) or "match" not in entry:
            sys.stderr.write(f"rootexec: policy[{i}] missing 'match'\n")
            sys.exit(3)
        specs.append((re.compile(entry["match"]), entry.get("desc", "")))
    return specs


def is_allowed(specs, argv):
    joined = " ".join(argv)
    for rx, desc in specs:
        if rx.fullmatch(joined):
            return True, desc
    return False, ""


def run(cmd_argv, dry_run, as_json, pw):
    if os.geteuid() == 0:
        run_argv = cmd_argv
        need_root = False
    else:
        # Escalate via sudo -S, feeding the password on stdin (piped, no
        # interactive prompt). This is the "root via python" channel: the
        # password is piped, never typed interactively, so a weak/untrusted
        # model cannot hang on a prompt -- it either has the password (env) or
        # it does not.
        run_argv = ["sudo", "-S"] + cmd_argv
        need_root = True

    if dry_run:
        out = {"action": "DRY_RUN", "would_run": run_argv}
        print(json.dumps(out) if as_json else f"[dry-run] {' '.join(run_argv)}")
        return 0

    if need_root:
        if not pw:
            msg = ("rootexec: no sudo password available -- set ROOTEXEC_PW "
                   "env var or pass --pw. Refusing to prompt interactively "
                   "(that is the dumb-model guard).")
            audit("NO_PW", " ".join(cmd_argv), msg)
            sys.stderr.write(msg + "\n")
            return 3
        p = subprocess.run(run_argv, input=(pw + "\n"),
                           text=True, stdout=None, stderr=None)
        return p.returncode

    p = subprocess.run(run_argv)
    return p.returncode


def main():
    ap = argparse.ArgumentParser(description="capability-gated root exec")
    ap.add_argument("--policy", default=DEFAULT_POLICY)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--pw", default=os.environ.get("ROOTEXEC_PW", ""),
                    help="sudo password (piped via sudo -S). Prefer ROOTEXEC_PW env.")
    ap.add_argument("command", nargs="*")
    args = ap.parse_args()

    specs = load_policy(args.policy)

    if args.list:
        for rx, desc in specs:
            print(f"{rx.pattern}\n    -> {desc}")
        return 0

    if args.self_test:
        demo = [
            (["apt-get", "install", "-y", "ccache"], "should be ALLOWED"),
            (["rm", "-rf", "/"], "should be BLOCKED"),
            (["sudo", "rm", "-rf", "/"], "should be BLOCKED"),
        ]
        for argv, note in demo:
            ok, desc = is_allowed(specs, argv)
            verdict = "ALLOW" if ok else "BLOCK"
            print(f"[{verdict}] {' '.join(argv)}  ({note})")
        return 0

    if not args.command:
        # command consumed by argparse? it stops at '--'
        ap.print_usage(sys.stderr)
        sys.exit(3)

    argv = args.command
    ok, desc = is_allowed(specs, argv)
    if not ok:
        audit("BLOCKED", " ".join(argv), "not in allowlist")
        msg = (f"rootexec: BLOCKED -- command not in capability allowlist: "
               f"{' '.join(argv)!r}\n"
               f"(this is the dumb-model guard; add it to the policy to permit)")
        if args.json:
            print(json.dumps({"decision": "BLOCKED", "cmd": argv}))
        else:
            sys.stderr.write(msg + "\n")
        sys.exit(2)

    audit("ALLOWED", " ".join(argv), desc)
    rc = run(argv, args.dry_run, args.json, args.pw)
    sys.exit(rc)


if __name__ == "__main__":
    main()
