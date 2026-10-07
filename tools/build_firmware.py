"""Build all public drivers from committed sources and bind outputs to those inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from release_bundle import ENVIRONMENTS, firmware_source_hashes

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project-dir', type=Path, default=ROOT, help='May be an ASCII junction to this same checkout')
    args = parser.parse_args()
    if args.project_dir.resolve() != ROOT:
        raise SystemExit('Project directory must refer to this same checkout')
    commit = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    if subprocess.check_output(['git','status','--porcelain','--untracked-files=all'],cwd=ROOT,text=True).strip():
        raise SystemExit('Commit verified source changes before preparing the firmware candidate')
    sources = firmware_source_hashes(ROOT)
    uvx = os.environ.get('J_CALENDAR_UVX') or shutil.which('uvx')
    command = [uvx,'--from','platformio==6.2.0','platformio'] if uvx else [sys.executable,'-m','platformio']
    core_version = subprocess.check_output(command+['--version'],cwd=ROOT,text=True).strip()
    if not core_version.endswith('version 6.2.0'):
        raise SystemExit('PlatformIO 6.2.0 is required; got '+core_version)
    command += ['run','--project-dir',str(args.project_dir)]
    for environment in ENVIRONMENTS: command += ['-e',environment]
    subprocess.run(command,cwd=ROOT,check=True)
    if sources != firmware_source_hashes(ROOT) or commit != subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip():
        raise SystemExit('Source changed during firmware build; rebuild before signing a receipt')
    outputs = {}
    for environment in ENVIRONMENTS:
        outputs[environment] = {}
        for name in ('bootloader.bin','partitions.bin','firmware.bin','firmware.elf'):
            path = ROOT/'.pio/build'/environment/name
            if not path.is_file(): raise SystemExit('Missing firmware output: '+environment+'/'+name)
            outputs[environment][name] = hashlib.sha256(path.read_bytes()).hexdigest()
    receipt = {'schema':1,'sourceCommit':commit,'createdAt':datetime.now(timezone.utc).isoformat(),
               'platformioVersion':core_version,'sources':sources,'outputs':outputs,
               'hardwareValidated':False,'buildCommand':command}
    destination = ROOT/'.pio/build/build-receipt.json'
    temporary = destination.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(receipt,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    temporary.replace(destination)
    print('Firmware build receipt:',destination)

if __name__ == '__main__': main()
