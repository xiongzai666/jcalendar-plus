"""Reproducible firmware and application verification; emits no device credentials or NVS data."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--skip-build', action='store_true', help='Use an already verified firmware build')
parser.add_argument('--environment', choices=['z15','z21','z98','1680'], default='z15')
args = parser.parse_args()

def run(command, cwd=ROOT):
    print('Verify:', ' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, check=True)

node = os.environ.get('J_CALENDAR_NODE') or shutil.which('node')
if not node:
    raise SystemExit('Node.js is required for verification')
# Drawing regression uses the same selected-driver font/dependency sources as firmware.
if not args.skip_build:
    if shutil.which('uvx'):
        run(['uvx', '--from', 'platformio==6.2.0', 'platformio', 'run', '-e', args.environment])
    else:
        run([__import__('sys').executable, '-m', 'platformio', 'run', '-e', args.environment])
os.environ['J_CALENDAR_ENVIRONMENT'] = args.environment
run([__import__('sys').executable, '-m', 'unittest', 'discover', '-s', 'test', '-p', 'test_*.py'])
run([sys.executable, 'tools/test_review_fixes.py'])
run([node, '--test'], ROOT / 'remote')
run([node, 'tools/check-build.mjs'], ROOT / 'remote')

cpp = os.environ.get('J_CALENDAR_CXX') or shutil.which('g++')
if not cpp:
    raise SystemExit('Set J_CALENDAR_CXX to g++ or zig for C++ policy tests')
compiler = [cpp, 'c++'] if Path(cpp).stem == 'zig' else [cpp]
with tempfile.TemporaryDirectory(prefix='jcalendar-verify-') as directory:
    tests = ['lesson_clock', 'schedule_optimization', 'effective_schedule', 'weather_cache',
             'calendar_override', 'display_policy', 'custom_date', 'bounded_string', 'remote_contract', 'network_budget', 'http_body', 'deadline_client', 'dns_candidates', 'owned_task', 'task_start', 'bounded_gzip', 'qweather_v1', 'public_defaults', 'ota_image', 'ota_upload']
    for name in tests:
        binary = Path(directory) / (name + ('.exe' if os.name == 'nt' else ''))
        extra = []
        if name == 'ota_upload':
            extra += ['-DSI_DRIVER=15', '-Itest/ota_upload_stub', 'src/local_ota.cpp']
        if name == 'bounded_gzip':
            uzlib = ROOT / 'lib/BoundedUZlib/src'
            extra.append('-I' + str(uzlib))
            ccompiler = [cpp, 'cc'] if Path(cpp).stem == 'zig' else [cpp, '-x', 'c']
            for source in ['tinflate', 'tinfgzip', 'crc32', 'adler32']:
                obj = Path(directory) / (source + '.o')
                run(ccompiler + ['-std=c99', '-I' + str(uzlib), '-c', uzlib / ('bounded_' + source + '.c'), '-o', obj])
                extra.append(obj)
            symbols = Path(directory) / ('gzip_symbols.exe' if os.name == 'nt' else 'gzip_symbols')
            run(ccompiler + ['-std=c99', '-I' + str(uzlib), 'test/gzip_symbols_host.c',
                '-x', 'none', Path(directory) / 'crc32.o', Path(directory) / 'adler32.o', '-o', symbols])
            run([symbols])
        run(compiler + ['-std=c++11', '-Iinclude', f'-I.pio/libdeps/{args.environment}/ArduinoJson/src',
                        f'test/{name}_host.cpp'] + extra + ['-o', binary])
        run([binary] + (['test/fixtures/remote-config.json'] if name == 'remote_contract' else []))

run([sys.executable, 'tools/test_ota_multipart.py'])

firmware = ROOT / f'.pio/build/{args.environment}/firmware.bin'
capacity = 0x1e0000  # min_spiffs.csv app0 and app1
if not firmware.exists() or firmware.stat().st_size > capacity:
    raise SystemExit('Firmware image missing or larger than app partition')
ratio = firmware.stat().st_size / capacity
print(f'Firmware image: {firmware.stat().st_size} bytes; {ratio:.1%} of app partition')
if ratio >= 0.95:
    print('Warning: firmware leaves less than 5% app space; review additions before release')
print('All configured verification gates passed.')
