"""Build/inspect an offline .irisfw with the pinned official workspace helpers.

This packages files only. It never uploads to a service or connects to a device.
"""
import argparse
import json
from pathlib import Path
import sys

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace',type=Path,required=True)
    p.add_argument('--project',type=Path,required=True)
    p.add_argument('--idf-path',type=Path,required=True)
    args=p.parse_args()
    workspace=args.workspace.resolve();project=args.project.resolve()
    tool_root=workspace/'submodule/esp-mosaico-utils/mosaico-tools'
    sys.path.insert(0,str(tool_root/'tools'))
    from mosaico_cli.workspace import load_workspace
    from mosaico_cli.runtime import RunContext,run_idf_target
    from mosaico_cli.gateway import ensure_iris_tools
    from mosaico_cli.bundle_plan import inspect_bundle_plan
    context=RunContext(load_workspace(tool_root,start=workspace),'release-bundle')
    python,_=ensure_iris_tools(context)
    run_idf_target(context,idf_path=args.idf_path.resolve(),project=project,
                   build_dir=project/'build',target='system-update-bundle',
                   definitions={'ESP_IRIS_PYTHON':str(python)},timeout=3600)
    description=json.loads((project/'build/project_description.json').read_text(encoding='utf-8'))
    bundle=project/'build'/(description['project_name']+'-system-update.irisfw')
    plan=inspect_bundle_plan(context,bundle)
    output=dict(bundle=str(bundle),log=str(context.log_path),plan=plan)
    (project/'build/release-bundle-plan.json').write_text(json.dumps(output,indent=2),encoding='utf-8')
    print(json.dumps(output,indent=2))

if __name__=='__main__':main()
