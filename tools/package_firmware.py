"""Build/inspect an offline .irisfw with the pinned official workspace helpers.

This packages files only. It never uploads to a service or connects to a device.
"""
import argparse
import json
from pathlib import Path
import sys

IDEAS_SCHEMA='esp-iris-system-update/0.2'

def require_schema(actual,expected=IDEAS_SCHEMA):
    if actual!=expected:
        raise ValueError(
            f'Official SDK exports {actual!r}; requested {expected!r}. '
            'Current Mosaico Ideas requires the official 0.2 SDK and Recovery ABI 2 '
            'application integration. Rebuild with the matching SDK; never edit '
            'the manifest schema to disguise an ABI 1 application. '
            'Use --legacy-v1 only for an explicitly local legacy-device export.')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--workspace',type=Path,required=True)
    p.add_argument('--project',type=Path,required=True)
    p.add_argument('--idf-path',type=Path,required=True)
    p.add_argument('--legacy-v1',action='store_true',
                   help='Explicit local ABI 1 export; incompatible with current Mosaico Ideas')
    args=p.parse_args()
    workspace=args.workspace.resolve();project=args.project.resolve()
    tool_root=workspace/'submodule/esp-mosaico-utils/mosaico-tools'
    sys.path.insert(0,str(tool_root/'tools'))
    from mosaico_cli.workspace import load_workspace
    from mosaico_cli.runtime import RunContext,run_idf_target
    from mosaico_cli.gateway import ensure_iris_tools
    from mosaico_cli.bundle_plan import inspect_bundle_plan
    from mosaico_cli.product_contract import validate_application_config
    configured=load_workspace(tool_root,start=workspace)
    expected='esp-iris-system-update/v1' if args.legacy_v1 else IDEAS_SCHEMA
    schema_path=configured.esp_iris_path/'components/esp_iris/protocol/system_update_manifest.schema.json'
    official_schema=json.loads(schema_path.read_text(encoding='utf-8'))['properties']['schema']['const']
    require_schema(official_schema,expected)
    context=RunContext(configured,'release-bundle')
    python,_=ensure_iris_tools(context)
    run_idf_target(context,idf_path=args.idf_path.resolve(),project=project,
                   build_dir=project/'build',target='system-update-bundle',
                   definitions={'ESP_IRIS_PYTHON':str(python)},timeout=3600)
    description=json.loads((project/'build/project_description.json').read_text(encoding='utf-8'))
    bundle=project/'build'/(description['project_name']+'-system-update.irisfw')
    plan=inspect_bundle_plan(context,bundle)
    require_schema(plan.get('schema'),expected)
    validate_application_config(project/'build')
    output=dict(bundle=str(bundle),log=str(context.log_path),plan=plan)
    (project/'build/release-bundle-plan.json').write_text(json.dumps(output,indent=2),encoding='utf-8')
    print(json.dumps(output,indent=2))

if __name__=='__main__':main()
