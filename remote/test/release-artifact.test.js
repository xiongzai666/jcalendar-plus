import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, writeFile, rm, mkdir, readFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { createHash } from 'node:crypto';
import { spawnSync } from 'node:child_process';

const sha = value => createHash('sha256').update(value).digest('hex');
async function fixture(t) {
  const dir = await mkdtemp(join(tmpdir(), 'jcalendar-public-release-'));
  t.after(() => rm(dir, { recursive: true, force: true }));
  return dir;
}
test('frozen artifact rejects changes after verification', async t => {
  const api = await import('../tools/release-artifact.mjs').catch(() => ({}));
  assert.equal(typeof api.verifyFrozenArchive, 'function', 'frozen public artifact gate is missing');
  const path = join(await fixture(t), 'artifact.zip');
  await writeFile(path, 'verified');
  await api.verifyFrozenArchive(path, sha('verified'));
  await writeFile(path, 'tampered');
  await assert.rejects(api.verifyFrozenArchive(path, sha('verified')), /hash|changed/i);
});
test('compiled ZIP preserves CLI layout and rejects unverified inputs', async t => {
  const { packArtifact } = await import('../tools/release-artifact.mjs');
  const dir = await fixture(t), root = join(dir, '.edgeone');
  await mkdir(join(root, 'cloud-functions/api-node'), { recursive: true });
  const name = 'cloud-functions/api-node/index.mjs';
  await writeFile(join(root, name), 'compiled');
  const config = join(dir, 'edgeone.json'); await writeFile(config, '{"outputDirectory":"public"}');
  const archive = join(dir, 'artifact.zip');
  await packArtifact(root, archive, { [name]: sha('compiled') }, config, sha(await readFile(config)));
  const python = process.env.J_CALENDAR_PYTHON || (process.platform === 'win32' ? 'py' : 'python3');
  const result = spawnSync(python,
    [...(python === 'py' ? ['-3'] : []), '-c',
      'import sys,json,zipfile;print(json.dumps(zipfile.ZipFile(sys.argv[1]).namelist()))', archive], { encoding: 'utf8' });
  assert.equal(result.status, 0, result.stderr);
  assert.deepEqual(JSON.parse(result.stdout).sort(), ['remote/.edgeone/' + name, 'remote/edgeone.json']);
  await assert.rejects(packArtifact(root, join(dir, 'bad.zip'), { [name]: sha('different') }, config, sha(await readFile(config))), /changed|hash/i);
  await assert.rejects(packArtifact(root, join(dir, 'escape.zip'), { '../outside': sha('compiled') }, config, sha(await readFile(config))), /input|path/i);
  await assert.rejects(packArtifact(root, join(dir, 'config.zip'), {}, config, sha('changed')), /config|changed/i);
});
