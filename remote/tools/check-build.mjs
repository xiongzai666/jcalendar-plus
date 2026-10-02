import { access, readdir, readFile, writeFile } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

for (const file of ['public/index.html', 'public/app.js', 'public/override-review.js', 'public/holiday-range.js', 'public/config-transfer.js', 'public/app.css', 'public/history.css', 'public/sync-status.css',
  'cloud-functions/api/[[default]].js', 'cloud-functions/lib/app.js']) {
  await access(new URL(`../${file}`, import.meta.url));
}
console.log('Static assets and Cloud Function entry are ready.');

const remote = new URL('../', import.meta.url);
const assets = {};
async function checkDirectory(directory) {
  for (const item of await readdir(directory, { withFileTypes: true })) {
    const path = new URL(item.name + (item.isDirectory() ? '/' : ''), directory);
    if (item.isDirectory()) await checkDirectory(path);
    else if (item.name.endsWith('.js')) {
      const result = spawnSync(process.execPath, ['--check', fileURLToPath(path)], { encoding: 'utf8' });
      if (result.status !== 0) throw new Error(result.stderr || `JavaScript syntax check failed: ${path.pathname}`);
      const source = await readFile(path, 'utf8');
      for (const match of source.matchAll(/(?:from\s*|import\s*)['"](\.\.?\/[^'"]+)['"]/g))
        await access(new URL(match[1], path));
    }
    if (!item.isDirectory() && path.pathname.includes('/public/') && item.name !== 'release.json')
      assets[item.name] = createHash('sha256').update(await readFile(path)).digest('hex');
  }
}
await checkDirectory(new URL('public/', remote));
await checkDirectory(new URL('cloud-functions/', remote));
const header = await readFile(new URL('../include/version.h', remote), 'utf8');
const version = /#define J_VERSION "([^"]+)"/.exec(header)?.[1];
if (!version) throw new Error('Firmware release version missing');
await writeFile(new URL('public/release.json', remote), JSON.stringify({ version, assets }, null, 2) + '\n');
console.log(`JavaScript syntax, imports and release manifest checked: ${version}`);
