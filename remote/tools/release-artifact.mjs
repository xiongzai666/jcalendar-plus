import { readFile, readdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { join } from 'node:path';
import { spawnSync } from 'node:child_process';

export const sha = value => createHash('sha256').update(value).digest('hex');
export async function files(root, prefix = '') {
  const result = [];
  for (const entry of await readdir(join(root, prefix), { withFileTypes: true })) {
    const name = prefix + entry.name;
    if (entry.isSymbolicLink()) throw new Error(`Unexpected symlink: ${name}`);
    if (entry.isDirectory()) result.push(...await files(root, name + '/'));
    else if (entry.isFile()) result.push(name);
    else throw new Error(`Unsupported file: ${name}`);
  }
  return result.sort();
}
export async function inventory(root) {
  const result = {};
  for (const name of await files(root)) result[name] = sha(await readFile(join(root, name)));
  return result;
}
export async function verifyFrozenArchive(path, expected) {
  if (!/^[a-f0-9]{64}$/.test(expected) || sha(await readFile(path)) !== expected)
    throw new Error('Archive hash changed; prepare again');
}
export async function packArtifact(root, archive, approved, config, configHash) {
  const code = `import sys,json,zipfile,pathlib,hashlib
root=pathlib.Path(sys.argv[1]).resolve()
contents={}
for n,h in json.load(sys.stdin).items():
 p=(root/n).resolve()
 if not p.is_relative_to(root) or '..' in pathlib.PurePosixPath(n).parts or pathlib.Path(root/n).is_symlink(): raise RuntimeError('Invalid archive input path')
 data=p.read_bytes()
 if hashlib.sha256(data).hexdigest()!=h: raise RuntimeError('Verified artifact hash changed')
 contents['remote/.edgeone/'+n]=data
config=pathlib.Path(sys.argv[3]).read_bytes()
if hashlib.sha256(config).hexdigest()!=sys.argv[4]: raise RuntimeError('Build configuration changed')
contents['remote/edgeone.json']=config
with zipfile.ZipFile(sys.argv[2],'x',zipfile.ZIP_DEFLATED) as z:
 for n,data in contents.items(): z.writestr(n,data)
`;
  const python = process.env.J_CALENDAR_PYTHON || (process.platform === 'win32' ? 'py' : 'python3');
  const result = spawnSync(python, [...(python === 'py' ? ['-3', '-X', 'utf8'] : []), '-c', code,
    root, archive, config, configHash], { input: JSON.stringify(approved), encoding: 'utf8' });
  if (result.error || result.status !== 0) throw new Error(`Artifact packaging failed: ${result.stderr || result.error}`);
}
