#!/usr/bin/env python3
"""Fail if MatchTemplate / FeatureMatch tasks require a PNG that is not on disk.

Mirrors TaskData.cpp generate_match_task_info: an explicit baseTask does NOT
inherit ``template``. The default is ``<TaskName>.png``. Missing files make
TemplResource::load return false and AsstLoadResource fails for every task.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
TASK_ROOT = REPO_ROOT / "resource" / "tasks"
TEMPLATE_ROOT = REPO_ROOT / "resource" / "template"

NON_TEMPLATE_ALGORITHMS = {"justreturn", "ocrdetect"}
TEMPLATE_ALGORITHMS = {"matchtemplate", "featurematch"}


def load_json(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    try:
        data = json.loads(text)
    except json.JSONDecodeError:
        # meojson accepts trailing commas; strip the last comma before } or ]
        stripped = []
        in_string = False
        escape = False
        for ch in text:
            if in_string:
                stripped.append(ch)
                if escape:
                    escape = False
                elif ch == "\\":
                    escape = True
                elif ch == '"':
                    in_string = False
                continue
            if ch == '"':
                in_string = True
                stripped.append(ch)
                continue
            stripped.append(ch)
        compact = "".join(stripped)
        while True:
            nxt = compact.replace(",}", "}").replace(",]", "]")
            if nxt == compact:
                break
            compact = nxt
        data = json.loads(compact)
    if not isinstance(data, dict):
        raise ValueError(f"{path}: top-level JSON must be an object")
    return data


def collect_tasks(task_root: Path) -> dict[str, dict]:
    tasks: dict[str, dict] = {}
    for path in sorted(task_root.rglob("*.json")):
        data = load_json(path)
        for name, spec in data.items():
            if not isinstance(spec, dict):
                continue
            tasks[name] = spec
    return tasks


def template_base_names(name: str) -> list[str]:
    """Suffixes after each '@', matching TaskData::generate_raw_task_and_base."""
    bases: list[str] = []
    start = 0
    while True:
        pos = name.find("@", start)
        if pos < 0:
            break
        bases.append(name[pos + 1 :])
        start = pos + 1
    return bases


def resolve_algorithm(
    name: str, tasks: dict[str, dict], visiting: set[str] | None = None
) -> str:
    visiting = visiting or set()
    if name in visiting:
        return "MatchTemplate"
    visiting = visiting | {name}
    spec = tasks.get(name, {})
    algo = spec.get("algorithm") if spec else None
    if isinstance(algo, str) and algo:
        return algo
    base = spec.get("baseTask") if spec else None
    if isinstance(base, str) and base:
        return resolve_algorithm(base, tasks, visiting)
    # Explicit `@` tasks inherit algorithm from the suffix even without baseTask.
    for suffix in template_base_names(name):
        if suffix in tasks:
            return resolve_algorithm(suffix, tasks, visiting)
    return "MatchTemplate"


def required_templates(name: str, spec: dict, algorithm: str) -> list[str]:
    if algorithm.lower() in NON_TEMPLATE_ALGORITHMS:
        return []
    if algorithm.lower() not in TEMPLATE_ALGORITHMS:
        # Default algorithm is MatchTemplate; unknown names still need a png.
        if algorithm.lower() not in {"", "matchtemplate"}:
            # FeatureMatch is already in TEMPLATE_ALGORITHMS. Anything else that
            # is not OCR/JustReturn still goes through match generation.
            pass
    tmpl = spec.get("template")
    if tmpl is None:
        return [f"{name}.png"]
    if isinstance(tmpl, str):
        return [tmpl]
    if isinstance(tmpl, list):
        return [str(item) for item in tmpl]
    return [f"{name}.png"]


def index_templates(template_root: Path) -> list[str]:
    rels: list[str] = []
    for path in template_root.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(template_root).as_posix()
        rels.append(rel)
    return rels


def template_exists(name: str, index: list[str]) -> bool:
    search = name.replace("\\", "/")
    if "." not in Path(search).name:
        search = f"{search}.png"
    for rel in index:
        if rel == search or rel.endswith(f"/{search}"):
            return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--task-root", type=Path, default=TASK_ROOT)
    parser.add_argument("--template-root", type=Path, default=TEMPLATE_ROOT)
    args = parser.parse_args()

    tasks = collect_tasks(args.task_root)
    index = index_templates(args.template_root)
    missing: list[str] = []
    for name, spec in sorted(tasks.items()):
        algorithm = resolve_algorithm(name, tasks)
        if algorithm.lower() in NON_TEMPLATE_ALGORITHMS:
            continue
        for tmpl in required_templates(name, spec, algorithm):
            if not template_exists(tmpl, index):
                base = spec.get("baseTask", "")
                missing.append(
                    f"{name}: template {tmpl!r} not found"
                    + (
                        f" (explicit baseTask={base!r} does not inherit template)"
                        if base
                        else ""
                    )
                )

    if missing:
        print("Missing task templates:", file=sys.stderr)
        for line in missing:
            print(f"  {line}", file=sys.stderr)
        print(f"{len(missing)} missing template(s)", file=sys.stderr)
        return 1

    print(f"OK: {len(tasks)} tasks, all MatchTemplate/FeatureMatch templates exist")
    return 0


if __name__ == "__main__":
    sys.exit(main())
