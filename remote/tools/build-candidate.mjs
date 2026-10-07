// Local compilation and isolated acceptance only. Never loads remote/.env or uploads.
import { readFile, writeFile, mkdir, copyFile, cp, mkdtemp } from 'node:fs/promises';
import { openSync, closeSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { join, dirname, resolve, delimiter } from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import { files, inventory, sha, packArtifact, verifyFrozenArchive } from './release-artifact.mjs';

const remote = fileURLToPath(new URL('../', import.meta.url));
const { values } = parseArgs({ options: { 'cli-entry': { type: 'string' }, output: { type: 'string' } } });
const cli = values['cli-entry'] || process.env.EDGEONE_CLI_ENTRY;
if (!cli) throw new Error('Set EDGEONE_CLI_ENTRY or --cli-entry to the installed edgeone.js');
await readFile(cli);
const names = ['package.json', 'package-lock.json', 'edgeone.json', 'tools/check-build.mjs',
  'tools/build-candidate.mjs', 'tools/release-artifact.mjs', 'test-support/edgeone-handler.mjs',
  'test/release-bundle-entry.test.js', '../test/fixtures/remote-config.json'];
const directoryEntries = {};
for (const directory of ['public', 'cloud-functions']) {
  directoryEntries[directory] = (await files(join(remote, directory))).filter(name => name !== 'release.json');
  for (const name of directoryEntries[directory]) {
    if (name === 'release.json') continue;
    if (name.split('/').some(part => part.startsWith('.')) ||
        !(directory === 'cloud-functions' ? /\.js$/ : /\.(html|js|css|svg)$/).test(name))
      throw new Error(`Unapproved source input: ${directory}/${name}`);
    names.push(directory + '/' + name);
  }
}
async function sourceManifest() {
  const hashes = {};
  for (const name of names.sort()) hashes[name] = sha(await readFile(join(remote, name)));
  hashes['../include/version.h'] = sha(await readFile(join(remote, '../include/version.h')));
  return hashes;
}
const sources = await sourceManifest();
const root = resolve(values.output || join(remote, '../.releases/public-candidates'));
await mkdir(root, { recursive: true });
const directory = await mkdtemp(join(root, 'edgeone-'));
const snapshot = join(directory, 'remote'), log = join(directory, 'build.log');
for (const name of names) {
  const destination = join(snapshot, name);
  await mkdir(dirname(destination), { recursive: true });
  await copyFile(join(remote, name), destination);
  if (sha(await readFile(destination)) !== sources[name]) throw new Error('Source changed while snapshotting; prepare again');
}
await mkdir(join(directory, 'include'), { recursive: true });
await copyFile(join(remote, '../include/version.h'), join(directory, 'include/version.h'));
if (sha(await readFile(join(directory, 'include/version.h'))) !== sources['../include/version.h'])
  throw new Error('Firmware version changed while snapshotting; prepare again');
await cp(join(remote, 'node_modules/@edgeone/pages-blob'), join(snapshot, 'node_modules/@edgeone/pages-blob'), { recursive: true });
// Empty build inputs; runtime fixture secrets are injected only into the isolated VM.
await writeFile(join(snapshot, '.env'), 'PUBLIC_ORIGIN=https://config.example.test\n');
const isolatedEnv = {};
for (const key of ['PATH', 'Path', 'SystemRoot', 'WINDIR', 'TEMP', 'TMP', 'HOME', 'USERPROFILE', 'APPDATA', 'LOCALAPPDATA'])
  if (process.env[key]) isolatedEnv[key] = process.env[key];
Object.assign(isolatedEnv, { HTTP_PROXY: process.env.HTTP_PROXY ?? 'http://127.0.0.1:7897',
  HTTPS_PROXY: process.env.HTTPS_PROXY ?? 'http://127.0.0.1:7897', NO_PROXY: 'localhost,127.0.0.1,::1' });
// CLI child npm/node must use the same runtime as this command.
delete isolatedEnv.Path;
isolatedEnv.PATH = dirname(process.execPath) + delimiter + (process.env.PATH || process.env.Path || '');
function run(entry, args, cwd, extra = {}) {
  const descriptor = openSync(log, 'a');
  let result;
  try { result = spawnSync(process.execPath, [entry, ...args], { cwd,
    env: { ...isolatedEnv, ...extra }, stdio: ['ignore', descriptor, descriptor] }); }
  finally { closeSync(descriptor); }
  if (result.error || result.status !== 0) throw new Error(`Candidate compilation/acceptance failed; inspect ${log}`);
}
run(cli, ['makers', 'build', '--mode', 'prod'], snapshot);
const artifact = join(snapshot, '.edgeone');
const bundle = join(artifact, 'cloud-functions/api-node/index.mjs');
const routes = JSON.parse(await readFile(join(dirname(bundle), 'config.json'), 'utf8'));
if (JSON.stringify(routes) !== JSON.stringify({ routes: [{ src: '^/api/(.*)$' }] }))
  throw new Error('Unexpected compiled API route mapping');
run('--test', [join(remote, 'test/release-bundle-entry.test.js')], remote, { JC_EDGEONE_BUNDLE: bundle });
const artifacts = await inventory(artifact);
for (const name of Object.keys(sources).filter(name => name.startsWith('public/'))) {
  const asset = name.replace(/^public\//, 'assets/');
  if (artifacts[asset] !== sources[name]) throw new Error(`Compiled static asset differs: ${name}`);
}
if (JSON.stringify(sources) !== JSON.stringify(await sourceManifest())) throw new Error('Sources changed during build; prepare again');
for (const directory of ['public', 'cloud-functions'])
  if (JSON.stringify(directoryEntries[directory]) !== JSON.stringify((await files(join(remote, directory))).filter(name => name !== 'release.json')))
    throw new Error('Source files added or removed during build; prepare again');
const archive = join(directory, 'artifact.zip');
await packArtifact(artifact, archive, artifacts, join(snapshot, 'edgeone.json'), sources['edgeone.json']);
const archiveSha256 = sha(await readFile(archive));
await verifyFrozenArchive(archive, archiveSha256);
const git = args => spawnSync('git', args, { cwd: remote, encoding: 'utf8' });
const head = git(['rev-parse', 'HEAD']);
if (head.status !== 0) throw new Error('Cannot identify source commit');
const cliPackage = JSON.parse(await readFile(resolve(cli, '../../package.json'), 'utf8'));
const receipt = { schema: 1, fixtureOnly: true, deployable: false, createdAt: new Date().toISOString(),
  sourceCommit: head.stdout.trim(), sourceDirty: Boolean(git(['status', '--porcelain']).stdout.trim()), sources,
  nodeVersion: process.version, cliVersion: cliPackage.version, cliEntrySha256: sha(await readFile(cli)),
  blobDependency: JSON.parse(await readFile(join(snapshot, 'node_modules/@edgeone/pages-blob/package.json'), 'utf8')).version,
  artifactFiles: artifacts, archiveLayout: 'remote/.edgeone/* + remote/edgeone.json', archiveSha256,
  verification: 'complete compiled server: authentication, real Blob SDK against local fixture transport, storage, schema gates, device projection, acknowledgement, history and restore',
  unverified: ['EdgeOne preview/production runtime and real Blob service', 'physical device acceptance'] };
await writeFile(join(directory, 'receipt.json'), JSON.stringify(receipt, null, 2) + '\n');
console.log(`Fixture candidate: ${directory}\nZIP SHA256: ${archiveSha256}\nNo deployment performed.`);
