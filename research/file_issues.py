#!/usr/bin/env python3
# Files GitHub issues for the top items of each workstream in research/office_docket.md.
# Uses the repo token discovered from `git remote get-url origin`.
# Idempotent-ish: skips issue titles that already exist (fetched once at start).
import subprocess, re, urllib.request, json, sys

REPO = "waefrebeorn/WuBuOffice"
API = f"https://api.github.com/repos/{REPO}"

def get_token():
    url = subprocess.check_output(["git","remote","get-url","origin"], text=True).strip()
    m = re.search(r"//([^@]+)@", url)
    cred = m.group(1)
    return cred.split(":",1)[1] if ":" in cred else cred

TOK = get_token()
HDR = {"Authorization": "token "+TOK, "Accept": "application/vnd.github+json",
       "Content-Type": "application/json", "User-Agent": "wubu-issue-filer"}

def api(method, path, data=None):
    req = urllib.request.Request(API+path, headers=HDR, method=method)
    if data is not None:
        req.data = json.dumps(data).encode()
    try:
        r = urllib.request.urlopen(req)
        return json.load(r)
    except urllib.error.HTTPError as e:
        body = e.read().decode()
        raise RuntimeError(f"{method} {path} -> {e.code}: {body[:400]}")

def existing_titles():
    titles = set()
    page = 1
    while True:
        res = api("GET", f"/issues?state=open&per_page=100&page={page}")
        if not res:
            break
        for it in res:
            titles.add(it.get("title",""))
        page += 1
    return titles

# 22 workstream labels (color-coded by cluster)
LABELS = {
 "WS01-pricing": "0e8a16",
 "WS02-privacy": "1f6feb",
 "WS03-ui": "bf3989",
 "WS04-a11y": "d4a72c",
 "WS05-formats": "5319e7",
 "WS06-collab": "0969da",
 "WS07-perf": "fb8500",
 "WS08-spreadsheet": "cf222e",
 "WS09-word": "8250df",
 "WS10-presentations": "1a7f37",
 "WS11-model": "088391",
 "WS12-extensibility": "bc4c00",
 "WS13-ai": "0969da",
 "WS14-rl": "388bfd",
 "WS15-os": "218bff",
 "WS16-crossapp": "6f42c1",
 "WS17-knowledge": "2da44e",
 "WS18-security": "b62324",
 "WS19-migration": "c93411",
 "WS20-distribution": "9a6700",
 "WS21-mobile": "e37370",
 "WS22-testing": "57606a",
}

def ensure_labels():
    for name, color in LABELS.items():
        try:
            api("POST", "/labels", {"name": name, "color": color})
            print("created label", name)
        except RuntimeError as e:
            if "already_exists" in str(e) or "422" in str(e):
                pass  # exists
            else:
                print("label warn", name, e)

# Parse the top N items per workstream from the docket.
def parse_top(path="research/office_docket.md", per=3):
    cur_ws = None
    collected = {}
    with open(path) as f:
        for line in f:
            m = re.match(r"^### (\d{2}\. .+)$", line)
            if m:
                cur_ws = m.group(1).strip()
                collected[cur_ws] = []
                continue
            mi = re.match(r"^\d{4,}\. (.+)$", line)
            if mi and cur_ws and len(collected[cur_ws]) < per:
                collected[cur_ws].append(mi.group(1).strip())
    return collected

def label_for_ws(ws_title):
    num = ws_title.split(".")[0]
    key = list(LABELS.keys())[int(num)-1]
    return key

ACCEPT = ("**Acceptance criteria**\n"
          "- Feature ships offline with zero required account/telemetry.\n"
          "- Covered by an automated test in the WS22 correctness suite.\n"
          "- Documented for users and (if applicable) for extension developers.\n"
          "- Source: WuBuOffice research docket (`research/office_docket.md`).")

def file_issues(top, existing):
    n = 0
    for ws_title, items in top.items():
        lbl = label_for_ws(ws_title)
        for it in items:
            title = f"[{ws_title.split('.')[0]}] {it[:80]}"
            if title in existing:
                print("skip (exists):", title)
                continue
            body = (f"**Workstream:** {ws_title}\n\n"
                    f"**Docket item:** {it}\n\n"
                    f"{ACCEPT}\n")
            try:
                api("POST", "/issues", {"title": title, "body": body, "labels": [lbl]})
                n += 1
                print("filed:", title)
            except RuntimeError as e:
                print("ERR filing", title, e)
    return n

if __name__ == "__main__":
    ensure_labels()
    top = parse_top(per=3)  # 22*3 = 66 issues; cap via env if needed
    existing = existing_titles()
    # limit to 50 if PER env set
    cap = int(__import__("os").environ.get("PER_WS","3"))
    if cap != 3:
        top = parse_top(per=cap)
    n = file_issues(top, existing)
    print(f"DONE: filed {n} new issues.")
