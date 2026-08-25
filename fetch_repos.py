import json
import os
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent


def run(cmd, cwd=ROOT):
    print("+", " ".join(cmd))
    subprocess.check_call(cmd, cwd=cwd)


def main():
    repos = json.loads((ROOT / "repos.json").read_text(encoding="utf-8"))
    for repo in repos:
        target = ROOT / repo["path"]
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            run(["git", "fetch", "--depth", "1", "origin", repo["branch"]], cwd=target)
            run(["git", "checkout", repo["branch"]], cwd=target)
        else:
            run(["git", "clone", "--depth", "1", "--branch", repo["branch"], repo["url"], str(target)])


if __name__ == "__main__":
    main()

