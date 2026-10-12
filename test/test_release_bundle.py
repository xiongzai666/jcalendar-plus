import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
import struct
import hashlib
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_bundle', ROOT/'tools/release_bundle.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
def partitions(app1=0x1e0000):
 return b''.join(struct.pack('<HBBII16sI',0x50aa,0,subtype,address,size,name,0)
  for subtype,address,size,name in ((0x10,0x10000,0x1e0000,b'app0'),(0x11,0x1f0000,app1,b'app1')))+b'\xff'*32
class ReleaseBundleTests(unittest.TestCase):
 def test_source_archive_uses_tracked_project_files_and_source_dependencies_only(self):
  from unittest.mock import patch
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);home=root/'pio-home';(root/'include').mkdir();(root/'include/version.h').write_bytes(b'version')
   (root/'nvs.bin').write_bytes(b'private')
   for directory in (root/'.pio/libdeps/z15',home/'packages/framework-arduinoespressif32',home/'platforms/espressif32'):
    directory.mkdir(parents=True);(directory/'public.cpp').write_bytes(b'source');(directory/'compiled.a').write_bytes(b'binary')
   wrapper=root/'.pio/libdeps/z15/ArduinoUZlib';wrapper.mkdir();(wrapper/'unknown.cpp').write_bytes(b'unlicensed-wrapper')
   with patch.object(module.subprocess,'check_output',return_value=b'include/version.h\0'):
    bundle=module.package_source(root,root/'out','2.3.2-rc.3','a'*40,home)
   with zipfile.ZipFile(bundle) as archive:
    names=archive.namelist()
    self.assertIn('include/version.h',names)
    self.assertIn('third_party/libraries/public.cpp',names)
    self.assertNotIn('nvs.bin',names)
    self.assertFalse(any(name.endswith('.a') or 'ArduinoUZlib/' in name for name in names))
    manifest=json.loads(archive.read('SOURCE_MANIFEST.json'))
    self.assertEqual(manifest['sourceCommit'],'a'*40)
    for name,expected in manifest['files'].items():
     self.assertEqual(hashlib.sha256(archive.read(name)).hexdigest(),expected)
 def test_bundle_contains_verified_notices_and_corresponding_source_instructions(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'licenses').mkdir();(root/'docs').mkdir();build=root/'build';build.mkdir()
   for name in ('LICENSE','NOTICE.md','docs/THIRD_PARTY_LICENSES.md','docs/RELEASE_SOURCE.md'):
    (root/name).write_bytes(b'notice-'+name.encode())
   font=root/'licenses/font.txt';font.write_bytes(b'font-license')
   (root/'licenses/PROVENANCE.json').write_text(json.dumps({'attachments':[{'file':'licenses/font.txt','sha256':hashlib.sha256(font.read_bytes()).hexdigest()}]}))
   for name in ('bootloader.bin','firmware.bin','boot_app0.bin'):(build/name).write_bytes(b'data')
   (build/'partitions.bin').write_bytes(partitions())
   bundle=module.package_environment(build,root/'out','z15','2.3.2-rc.3','a'*40,build/'boot_app0.bin',source_root=root)
   with zipfile.ZipFile(bundle) as archive:
    self.assertEqual(archive.read('licenses/font.txt'),b'font-license')
    self.assertEqual(archive.read('LICENSE'),b'notice-LICENSE')
    self.assertIn('docs/RELEASE_SOURCE.md',archive.namelist())
    self.assertIn('licenses/font.txt',archive.read('SHA256SUMS.txt').decode())
   font.write_bytes(b'changed-license')
   with self.assertRaisesRegex(ValueError,'license|attachment'):
    module.package_environment(build,root/'out','z15','2.3.2-rc.3','a'*40,build/'boot_app0.bin',source_root=root)
 def test_release_refuses_stale_build_commit_sources_or_segments(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);(root/'include').mkdir();build=root/'.pio/build';(build/'z15').mkdir(parents=True)
   (root/'include/version.h').write_text('fixture-version',encoding='utf-8')
   (root/'platformio.ini').write_text('fixture-platform',encoding='utf-8')
   (build/'z15/firmware.bin').write_bytes(b'verified-firmware')
   sha=lambda data:hashlib.sha256(data).hexdigest()
   receipt={'sourceCommit':'a'*40,'sources':{'include/version.h':sha(b'fixture-version'),'platformio.ini':sha(b'fixture-platform')},
    'outputs':{'z15':{'firmware.bin':sha(b'verified-firmware')}}}
   path=build/'build-receipt.json';path.write_text(json.dumps(receipt),encoding='utf-8')
   module.verify_build_receipt(root,build,'z15','a'*40)
   with self.assertRaisesRegex(ValueError,'commit|source'):
    module.verify_build_receipt(root,build,'z15','b'*40)
   (root/'include/version.h').write_text('changed',encoding='utf-8')
   with self.assertRaisesRegex(ValueError,'source'):
    module.verify_build_receipt(root,build,'z15','a'*40)
   (root/'include/version.h').write_text('fixture-version',encoding='utf-8')
   (build/'z15/firmware.bin').write_bytes(b'changed')
   with self.assertRaisesRegex(ValueError,'segment|output'):
    module.verify_build_receipt(root,build,'z15','a'*40)
 def test_dependency_receipt_records_resolved_versions_and_file_hashes(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);package=root/'libdeps'/'Example';package.mkdir(parents=True)
   metadata={'name':'Example','version':'1.2.3','spec':{'owner':'example','uri':None}}
   (package/'.piopm').write_text(json.dumps(metadata),encoding='utf-8')
   receipt=module.dependency_inventory(root)
   self.assertEqual(receipt['libdeps/Example']['name'],'Example')
   self.assertEqual(receipt['libdeps/Example']['version'],'1.2.3')
   self.assertEqual(len(receipt['libdeps/Example']['metadataSha256']),64)
 def test_bundle_is_addressed_and_contains_only_release_segments(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);build=root/'build';build.mkdir();out=root/'out'
   for name in ('bootloader.bin','partitions.bin','firmware.bin'):(build/name).write_bytes(b'fake-'+name.encode())
   (build/'partitions.bin').write_bytes(partitions())
   (build/'nvs.bin').write_bytes(b'private')
   (build/'boot_app0.bin').write_bytes(b'ota')
   bundle=module.package_environment(build,out,'z15','2.3.1-rc.1','a'*40,build/'boot_app0.bin')
   self.assertEqual(bundle.name,'jcalendar-plus-2.3.1-rc.1-z15.zip')
   with zipfile.ZipFile(bundle) as archive:
    self.assertNotIn('nvs.bin',archive.namelist())
    manifest=json.loads(archive.read('manifest.json'))
    self.assertEqual(manifest['sourceCommit'],'a'*40)
    self.assertEqual(manifest['segments']['firmware.bin']['address'],'0x10000')
    self.assertEqual(manifest['segments']['boot_app0.bin']['address'],'0xe000')
    self.assertEqual(len(manifest['segments']['firmware.bin']['sha256']),64)
    self.assertIn('SHA256SUMS.txt',archive.namelist())
 def test_wrong_ota_partition_layout_is_rejected_before_packaging(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   for name in ('bootloader.bin','boot_app0.bin','firmware.bin'):(root/name).write_bytes(b'data')
   for table in (b'not-a-partition-table',partitions(app1=0x100000)):
    (root/'partitions.bin').write_bytes(table)
    with self.assertRaisesRegex(ValueError,'partition|OTA'):
     module.package_environment(root,root/'out','z15','2.3.0','a'*40,root/'boot_app0.bin')
 def test_missing_or_oversized_app_is_rejected(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d)
   with self.assertRaises(ValueError):module.package_environment(root,root/'out','z15','2.3.0','a'*40,root/'boot_app0.bin')
   for name in ('bootloader.bin','partitions.bin','boot_app0.bin'):(root/name).write_bytes(b'data')
   (root/'firmware.bin').write_bytes(b'x'*(module.APP_CAPACITY+1))
   with self.assertRaises(ValueError):module.package_environment(root,root/'out','z15','2.3.0','a'*40,root/'boot_app0.bin')
 def test_unknown_environment_is_rejected(self):
  with self.assertRaises(ValueError):module.package_environment(Path('.'),Path('.'),'esp32c3','2.3.0','a'*40,Path('none'))
if __name__=='__main__':unittest.main()
