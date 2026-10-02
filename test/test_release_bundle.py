import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('release_bundle', ROOT/'tools/release_bundle.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
class ReleaseBundleTests(unittest.TestCase):
 def test_bundle_is_addressed_and_contains_only_release_segments(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);build=root/'build';build.mkdir();out=root/'out'
   for name in ('bootloader.bin','partitions.bin','firmware.bin'):(build/name).write_bytes(b'fake-'+name.encode())
   (build/'nvs.bin').write_bytes(b'private')
   (build/'boot_app0.bin').write_bytes(b'ota')
   bundle=module.package_environment(build,out,'z15','2.3.0','a'*40,build/'boot_app0.bin')
   with zipfile.ZipFile(bundle) as archive:
    self.assertNotIn('nvs.bin',archive.namelist())
    manifest=json.loads(archive.read('manifest.json'))
    self.assertEqual(manifest['sourceCommit'],'a'*40)
    self.assertEqual(manifest['segments']['firmware.bin']['address'],'0x10000')
    self.assertEqual(manifest['segments']['boot_app0.bin']['address'],'0xe000')
    self.assertEqual(len(manifest['segments']['firmware.bin']['sha256']),64)
    self.assertIn('SHA256SUMS.txt',archive.namelist())
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
