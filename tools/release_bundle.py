"""Package build outputs only; never read device flash, NVS or private backups."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile
import struct
ENVIRONMENTS = ('z15', 'z21', 'z98', '1680')
APP_CAPACITY = 0x1e0000
ADDRESSES = {'bootloader.bin':'0x1000', 'partitions.bin':'0x8000', 'boot_app0.bin':'0xe000', 'firmware.bin':'0x10000'}

def verify_ota_partitions(data):
    apps = {}
    for offset in range(0, len(data)-31, 32):
        magic, kind, subtype, address, size, label, flags = struct.unpack('<HBBII16sI', data[offset:offset+32])
        if magic != 0x50aa: break
        if kind == 0: apps[subtype] = (address, size)
    if apps.get(0x10) != (0x10000, APP_CAPACITY) or apps.get(0x11) != (0x1f0000, APP_CAPACITY):
        raise ValueError('Unexpected OTA application partition layout; do not flash this bundle')

def dependency_inventory(root):
    result = {}
    for path in sorted(root.rglob('.piopm')):
        data = path.read_bytes()
        metadata = json.loads(data)
        result[path.parent.relative_to(root).as_posix()] = {
            'name':metadata['name'], 'version':metadata['version'],
            'source':metadata.get('spec'), 'metadataSha256':hashlib.sha256(data).hexdigest()}
    return result

def firmware_source_hashes(root):
    paths = [root/'platformio.ini']
    for directory in ('include','src','lib'):
        paths.extend(path for path in (root/directory).rglob('*') if path.is_file() and '.git' not in path.parts)
    return {path.relative_to(root).as_posix():hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(paths)}

def verify_build_receipt(root, build, environment, commit):
    try: receipt = json.loads((build/'build-receipt.json').read_text(encoding='utf-8'))
    except (OSError, ValueError) as cause: raise ValueError('Missing valid build receipt; run tools/build_firmware.py') from cause
    if receipt.get('sourceCommit') != commit or receipt.get('sources') != firmware_source_hashes(root):
        raise ValueError('Build source commit or inputs changed; rebuild before packaging')
    outputs = receipt.get('outputs',{}).get(environment)
    if not outputs or 'firmware.bin' not in outputs:
        raise ValueError('Missing verified environment output; rebuild before packaging')
    for name, expected in outputs.items():
        if name not in ('firmware.bin','bootloader.bin','partitions.bin','firmware.elf'):
            raise ValueError('Unapproved build output segment')
        path = build/environment/name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('Verified build output segment changed; rebuild before packaging')
    return receipt

def release_attachments(root):
    provenance = json.loads((root/'licenses/PROVENANCE.json').read_text(encoding='utf-8'))
    contents = {name:(root/name).read_bytes() for name in
        ('LICENSE','NOTICE.md','docs/THIRD_PARTY_LICENSES.md','docs/RELEASE_SOURCE.md','licenses/PROVENANCE.json')}
    for attachment in provenance['attachments']:
        name = attachment['file']
        data = (root/name).read_bytes()
        if hashlib.sha256(data).hexdigest() != attachment['sha256']:
            raise ValueError('License attachment hash changed: '+name)
        contents[name] = data
    return contents

def package_source(root, output, version, commit, platformio_home, library_environment='z15'):
    tracked = subprocess.check_output(['git','ls-files','-z'],cwd=root).decode('utf-8').split('\0')
    contents = {name:(root/name).read_bytes() for name in tracked if name}
    dependency_roots = {
        'third_party/libraries': root/'.pio/libdeps'/library_environment,
        'third_party/framework-arduinoespressif32': platformio_home/'packages/framework-arduinoespressif32',
        'third_party/platform-espressif32': platformio_home/'platforms/espressif32',
    }
    for prefix, directory in dependency_roots.items():
        for path in sorted(directory.rglob('*')):
            relative = path.relative_to(directory)
            if not path.is_file() or any(part in ('.git','__pycache__','ArduinoUZlib') for part in relative.parts):
                continue
            if path.suffix.lower() in ('.a','.o','.bin','.elf','.exe','.dll','.pyc') or relative.name == 'integrity.dat':
                continue
            contents[prefix+'/'+relative.as_posix()] = path.read_bytes()
    source_manifest = {
        'version':version,'sourceCommit':commit,'libraryEnvironment':library_environment,
        'files':{name:hashlib.sha256(data).hexdigest() for name,data in contents.items()},
        'dependencies':{prefix:dependency_inventory(directory) for prefix,directory in dependency_roots.items()},
        'excluded':'Git internals, compiled objects, device data; unused ArduinoUZlib wrapper omitted; actual decoder source in lib/BoundedUZlib',
        'platformSourceReferences':['https://github.com/espressif/arduino-esp32/tree/2.0.17','https://github.com/espressif/esp-idf/tree/v4.4.7'],
    }
    contents['SOURCE_MANIFEST.json']=(json.dumps(source_manifest,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
    output.mkdir(parents=True,exist_ok=True)
    archive=output/f'jcalendar-plus-{version}-source.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in contents.items(): z.writestr(name,data)
    return archive

def package_environment(build, output, environment, version, commit, boot_app0, dependencies=None, source_root=None):
    if environment not in ENVIRONMENTS: raise ValueError('Unsupported environment')
    paths = {name: (boot_app0 if name == 'boot_app0.bin' else build/name) for name in ADDRESSES}
    for name, path in paths.items():
        if not path.is_file() or not path.stat().st_size: raise ValueError(f'Missing build segment: {name}')
    capacities = {'bootloader.bin':0x7000, 'partitions.bin':0x1000, 'boot_app0.bin':0x2000, 'firmware.bin':APP_CAPACITY}
    if any(path.stat().st_size > capacities[name] for name,path in paths.items()):
        raise ValueError('Segment exceeds its flash region')
    verify_ota_partitions(paths['partitions.bin'].read_bytes())
    segments = {name:{'address':ADDRESSES[name], 'bytes':path.stat().st_size,
                'sha256':hashlib.sha256(path.read_bytes()).hexdigest()} for name,path in paths.items()}
    manifest = {'version':version, 'environment':environment, 'sourceCommit':commit,
                'chip':'esp32', 'flashBytes':4194304, 'segments':segments,
                'ota': {'applicationCapacity':APP_CAPACITY, 'imageFraction':paths['firmware.bin'].stat().st_size/APP_CAPACITY,
                        'layoutVerified':True, 'deviceUpgradeVerified':False},
                'hardwareValidation':'Z15 recorded first-setup, OTA, offline and 2026-10-13 aborted-OTA recovery evidence; rc.4 changes host-tested, not newly installed; other drivers build-only; no endurance or power claim'}
    if dependencies is not None: manifest['dependencies'] = dependencies
    output.mkdir(parents=True,exist_ok=True)
    archive = output/f'jcalendar-plus-{version}-{environment}.zip'
    guide = f"""# J-Calendar Plus {version} / {environment}
经典 ESP32、4 MB Flash；按实际屏幕型号选择驱动。
先检查 SHA256SUMS.txt。应用文件最大 1,966,080 字节。

首次刷机（仅全新设备；erase_flash 会清空 Wi-Fi、Token、课表等数据）：
python -m esptool --chip esp32 --port PORT erase_flash
python -m esptool --chip esp32 --port PORT write_flash 0x1000 bootloader.bin 0x8000 partitions.bin 0xe000 boot_app0.bin 0x10000 firmware.bin

升级：本机配置页 Update 上传 firmware.bin；不要上传整包、bootloader 或 partitions。
串口直接写 0x10000 仅适用于确定使用 app0、相同分区的设备；使用过 OTA 时优先网页更新。
未知原固件/分区先备份并参考仓库 docs/FLASHING.md，禁止盲目擦除。
本包只来自编译目录，未读取或包含设备 NVS/完整 Flash 备份。
Z15 有历史实机使用记录；其余仅构建验证，不保证任意 4.2 寸三色屏可用。
来源提交：{commit}
"""
    contents={name:path.read_bytes() for name,path in paths.items()}
    if source_root is not None:
        contents.update(release_attachments(source_root))
    contents['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode('utf-8')
    contents['FLASHING.md']=guide.encode('utf-8')
    sums=''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in contents.items())
    contents['SHA256SUMS.txt']=sums.encode('utf-8')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in contents.items():z.writestr(name,data)
    return archive

def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--environment',choices=ENVIRONMENTS,action='append')
    parser.add_argument('--boot-app0',type=Path,required=True,help='PlatformIO framework tools/partitions/boot_app0.bin')
    parser.add_argument('--output',type=Path,default=root/'.releases')
    parser.add_argument('--platformio-home',type=Path,default=Path.home()/'.platformio',help='Resolved PlatformIO package metadata for the build receipt')
    args=parser.parse_args()
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
    if subprocess.check_output(['git','status','--porcelain','--untracked-files=all'],cwd=root,text=True).strip():
        raise SystemExit('Commit the verified source tree before packaging')
    version=re.search(r'#define J_VERSION "([^"\n]+)"',(root/'include/version.h').read_text()).group(1)
    for environment in args.environment or ENVIRONMENTS:
        verify_build_receipt(root,root/'.pio/build',environment,commit)
    tool_dependencies = {}
    for directory, names in (('packages',('framework-arduinoespressif32','tool-esptoolpy','toolchain-xtensa-esp32')),
                             ('platforms',('espressif32',))):
        tool_dependencies[directory] = {}
        for name in names:
            metadata = dependency_inventory(args.platformio_home/directory/name).get('.')
            if not metadata: raise ValueError('Missing resolved PlatformIO dependency: '+name)
            tool_dependencies[directory][name] = metadata
    tool_dependencies['checkedInLibraries'] = {
        path.relative_to(root).as_posix():hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted((root/'lib').rglob('*')) if path.is_file() and '.git' not in path.parts}
    archives=[package_environment(root/'.pio/build'/env,args.output,env,version,commit,args.boot_app0,
        {'platformio':tool_dependencies,'libraries':dependency_inventory(root/'.pio/libdeps'/env),
         'platformioIniSha256':hashlib.sha256((root/'platformio.ini').read_bytes()).hexdigest()},source_root=root)
        for env in args.environment or ENVIRONMENTS]
    archives.append(package_source(root,args.output,version,commit,args.platformio_home,(args.environment or ENVIRONMENTS)[0]))
    sums=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in archives)
    (args.output/'SHA256SUMS.txt').write_text(sums,encoding='utf-8')
    for archive in archives:print(archive)
if __name__=='__main__':main()
