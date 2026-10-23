# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KymoCore-Commercial
from __future__ import annotations
import argparse, json, sys
from pathlib import Path
from .generation import check, generate
from .migration import migrate
from .model import default_project, load_project, save_project
from .validation import validate
from .port_template import create_port_template


def parser() -> argparse.ArgumentParser:
    p=argparse.ArgumentParser(prog="kymo-configurator",description="Validate and generate KymoCore configuration without Qt")
    sub=p.add_subparsers(dest="command",required=True)
    for name in ("validate","check","generate"):
        q=sub.add_parser(name); q.add_argument("project",type=Path); q.add_argument("--profile")
        if name in ("check","generate"): q.add_argument("--out",type=Path,required=True)
        if name=="generate": q.add_argument("--force",action="store_true")
        q.add_argument("--json",action="store_true",dest="json_output")
    q=sub.add_parser("migrate"); q.add_argument("project",type=Path); q.add_argument("--to-schema",type=int,default=2); q.add_argument("--output",type=Path)
    q=sub.add_parser("new"); q.add_argument("path",type=Path); q.add_argument("--name",default="KymoProject")
    q=sub.add_parser("init-port"); q.add_argument("project",type=Path); q.add_argument("--out",type=Path,required=True)
    q=sub.add_parser("gui"); q.add_argument("project",type=Path,nargs="?"); q.add_argument("--mode",choices=("guided","professional")); q.add_argument("--lang",choices=("en","de"))
    return p


def main(argv=None) -> int:
    args=parser().parse_args(argv)
    if args.command=="gui":
        try: from .gui.app import run
        except ImportError as exc:
            print(f"GUI requires the optional PySide6 dependency: {exc}",file=sys.stderr); return 2
        return run(args.project,args.mode,args.lang)
    if args.command=="new":
        if args.path.exists(): print(f"Refusing to overwrite {args.path}",file=sys.stderr); return 2
        save_project(args.path,default_project(args.name)); print(f"Created {args.path}"); return 0
    if args.command=="init-port":
        try:
            doc=load_project(args.project); print(create_port_template(doc,args.out)); return 0
        except (ValueError,OSError) as exc: print(str(exc),file=sys.stderr); return 2
    if args.command=="migrate":
        try:
            doc=load_project(args.project); result=migrate(doc,args.to_schema)
            target=args.output or args.project.with_name(args.project.stem+f".schema{args.to_schema}"+args.project.suffix)
            save_project(target,result); print(f"Migrated copy written to {target}"); return 0
        except ValueError as exc: print(str(exc),file=sys.stderr); return 2
    try: project=load_project(args.project)
    except ValueError as exc: print(str(exc),file=sys.stderr); return 2
    profile=args.profile or project.get("active_profile","development")
    diagnostics=validate(project,profile)
    if args.command=="validate":
        if args.json_output: print(json.dumps([d.to_dict() for d in diagnostics],ensure_ascii=False,indent=2))
        else:
            for d in diagnostics: print(f"{d.severity.upper()} {d.code} {d.path}: {d.message}"+(f" — {d.suggestion}" if d.suggestion else ""))
        return 1 if any(d.severity=="error" for d in diagnostics) else 0
    if any(d.severity=="error" for d in diagnostics):
        for d in diagnostics:
            if d.severity=="error": print(f"{d.path}: {d.message}",file=sys.stderr)
        return 1
    if args.command=="generate":
        try: print(generate(project,profile,args.out,force=args.force)); return 0
        except (ValueError,OSError) as exc: print(str(exc),file=sys.stderr); return 2
    ok,message=check(project,profile,args.out)
    print(message); return 0 if ok else 1

if __name__=="__main__": raise SystemExit(main())
