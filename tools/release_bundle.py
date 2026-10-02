"""Package build outputs only; never read device flash, NVS or private backups."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile
ENVIRONMENTS = ('z15', 'z21', 'z98', '1680')
APP_CAPACITY = 0x1e0000
ADDRESSES = {'bootloader.bin':'0x1000', 'partitions.bin':'0x8000', 'boot_app0.bin':'0xe000', 'firmware.bin':'0x10000'}

def package_environment(build, output, environment, version, commit, boot_app0):
    if environment not in ENVIRONMENTS: raise ValueError('Unsupported environment')
    paths = {name: (boot_app0 if name == 'boot_app0.bin' else build/name) for name in ADDRESSES}
    for name, path in paths.items():
        if not path.is_file() or not path.stat().st_size: raise ValueError(f'Missing build segment: {name}')
    capacities = {'bootloader.bin':0x7000, 'partitions.bin':0x1000, 'boot_app0.bin':0x2000, 'firmware.bin':APP_CAPACITY}
    if any(path.stat().st_size > capacities[name] for name,path in paths.items()):
        raise ValueError('Segment exceeds its flash region')
    segments = {name:{'address':ADDRESSES[name], 'bytes':path.stat().st_size,
                'sha256':hashlib.sha256(path.read_bytes()).hexdigest()} for name,path in paths.items()}
    manifest = {'version':version, 'environment':environment, 'sourceCommit':commit,
                'chip':'esp32', 'flashBytes':4194304, 'segments':segments,
                'hardwareValidation':'historical Z15 hardware only; public defaults require device acceptance'}
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
    args=parser.parse_args()
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
    if subprocess.check_output(['git','status','--porcelain','--untracked-files=all'],cwd=root,text=True).strip():
        raise SystemExit('Commit the verified source tree before packaging')
    version=re.search(r'#define J_VERSION "([^"\n]+)"',(root/'include/version.h').read_text()).group(1)
    archives=[package_environment(root/'.pio/build'/env,args.output,env,version,commit,args.boot_app0) for env in args.environment or ENVIRONMENTS]
    sums=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in archives)
    (args.output/'SHA256SUMS.txt').write_text(sums,encoding='utf-8')
    for archive in archives:print(archive)
if __name__=='__main__':main()
