import { readFile } from 'node:fs/promises';
import { spawnSync } from 'node:child_process';
import { Script } from 'node:vm';
import { createRequire } from 'node:module';
import { Readable } from 'node:stream';
import { EventEmitter } from 'node:events';
import { pathToFileURL } from 'node:url';

const require = createRequire(import.meta.url);
const forbidden = () => { throw new Error('External network is forbidden in bundle verification'); };

// Validate the complete ESM, then execute all its runtime and application code.
// Only platform imports, HTTP listening and Blob transport are isolated.
export async function loadEdgeOneHandler(path) {
  const checked = spawnSync(process.execPath, ['--check', path], { encoding: 'utf8' });
  if (checked.error || checked.status !== 0) throw new Error('Complete bundle syntax check failed');
  const bundle = await readFile(path, 'utf8'), imports = [];
  const executable = bundle.replace(/^import\s+(.+?)\s+from\s+["']([^"']+)["'];?\s*$/gm, (_, binding, name) => {
    imports.push({ binding, name }); return '';
  }).replace(/^export default (.+);\s*$/gm, 'void ($1);')
    .replace(/^export\s*\{[^}]*\};?\s*$/gm, '')
    .replace(/^export\s+(const|let|var|function|class)\s/gm, '$1 ')
    .replaceAll('import.meta.url', JSON.stringify(pathToFileURL(path).href));
  if (/^\s*(?:import|export)\b/m.test(executable)) throw new Error('Unsupported bundle module layout');
  const bindings = [];
  for (const [index, { binding }] of imports.entries()) {
    if (binding.startsWith('{')) {
      for (const field of binding.slice(1,-1).split(',')) {
        const [key, alias = key] = field.trim().split(/\s+as\s+/);
        bindings.push(`const ${alias} = __importModules[${index}][${JSON.stringify(key)}];`);
      }
    } else if (/^\w+$/.test(binding)) bindings.push(`const ${binding} = __importModules[${index}];`);
    else throw new Error('Unsupported bundle import binding');
  }
  // ESM is strict, imports are immutable, and module-level this is undefined.
  // Keep these semantics while isolating platform modules in a normal Node VM.
  const script = new Script(`'use strict'; (function(){\n${bindings.join('\n')}\n${executable}\n}).call(undefined);`,
    { filename: 'verified-edgeone-bundle.mjs' });
  return async (request, env, store) => {
    let handler; const listeners = [];
    const http = { ...require('node:http'), request: forbidden, get: forbidden,
      createServer(fn) { handler = fn; return { listen(port) { listeners.push(port); }, on() {}, close() {} }; } };
    const safeRequire = name => {
      name = name.replace(/^node:/, '');
      if (name === 'http') return http;
      if (name === 'module') return { createRequire: () => safeRequire };
      if (!['crypto','url','path','events','stream','buffer','util','zlib','net','tls','https'].includes(name))
        throw new Error(`Unsupported bundle dependency: ${name}`);
      const value = require('node:' + name);
      if (['https','net','tls'].includes(name)) return { ...value,
        request: forbidden, get: forbidden, connect: forbidden, createConnection: forbidden, createServer: forbidden };
      return value;
    };
    const sandbox = {
      require: safeRequire, Buffer, Request, Response, Headers, URL, URLSearchParams,
      AbortSignal, AbortController, TextEncoder, TextDecoder, ReadableStream, Blob, DOMException,
      crypto: require('node:crypto').webcrypto, atob, btoa, structuredClone,
      setTimeout, clearTimeout, setInterval: forbidden, clearInterval,
      console: { log() {}, warn() {}, error() {} },
      process: { env: { ADMIN_PASSWORD_SCRYPT: '', SESSION_SECRET: '', DEVICE_TOKEN_SHA256: '',
        ...env,
        PAGES_PROJECT_ID: 'fixture-project', PAGES_BLOB_DEPLOY_CREDENTIAL: 'fixture-token' } },
      fetch: async (target, options = {}) => {
        const url = new URL(target);
        if (url.href === 'https://blob-sts.edgeone.site/') return Response.json({
          tmpSecretId: 'fixture-id', tmpSecretKey: 'fixture-key', sessionToken: 'fixture-session',
          expiredTime: 4102444800, appId: 'fixture', zoneId: 'zone', projectId: 'project',
          resourcePrefix: 'fixture/zone/project', cosOverseas: { bucket: 'fixture', region: 'fixture' },
        });
        if (['fixture-zone-project.blob.edgeone.site','fixture-zone-project.blob-nocache.edgeone.site'].includes(url.hostname)) {
          const prefix = '/fixture/zone/project/jcalendar/';
          if (!url.pathname.startsWith(prefix)) throw new Error('Unexpected Blob namespace');
          const key = decodeURIComponent(url.pathname.slice(prefix.length));
          if (options.method === 'GET') {
            const value = await store.get(key, { type: 'json', consistency: 'strong' });
            return value == null ? new Response('', { status: 404 }) : Response.json(value);
          }
          if (options.method === 'PUT') { await store.setJSON(key, JSON.parse(options.body)); return new Response(''); }
          if (options.method === 'DELETE') { await store.delete(key); return new Response(null, { status: 204 }); }
          throw new Error('Unexpected Blob operation');
        }

        return forbidden();
      },
    };
    sandbox.__importModules = imports.map(({ name }) => safeRequire(name));
    script.runInNewContext(sandbox);
    if (!handler) throw new Error('Bundle did not register its server entry');
    if (listeners.length !== 1 || listeners[0] !== 9000) throw new Error('Bundle must listen once on platform port 9000');
    const url = new URL(request.url), bytes = Buffer.from(await request.arrayBuffer());
    const req = Readable.from(bytes.length ? [bytes] : []);
    Object.assign(req, { url: url.pathname + url.search, method: request.method,
      headers: { ...Object.fromEntries(request.headers), host: url.host, 'eo-pages-host': url.host, 'x-forwarded-proto': url.protocol.slice(0,-1) }, socket: {} });
    const chunks = [], res = new EventEmitter();
    Object.assign(res, { statusCode: 200, headers: {},
      writeHead(status, headers) { this.statusCode = status; this.headers = headers || {}; },
      write(chunk) { chunks.push(Buffer.from(chunk)); return true; },
      end(chunk) { if (chunk) chunks.push(Buffer.from(chunk)); this.writableEnded = true; this.emit('finish'); },
      setHeader(key, value) { this.headers[key] = value; }, destroy: forbidden,
    });
    await handler(req, res);
    if (!res.writableEnded) throw new Error('Bundle entry did not complete its response');
    return new Response([204,304].includes(res.statusCode) ? null : Buffer.concat(chunks),
      { status: res.statusCode, headers: res.headers });
  };
}
