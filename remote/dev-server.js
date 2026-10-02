import { createHash, randomBytes, scryptSync } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import { resolve, sep } from 'node:path';
import { handleRequest } from './cloud-functions/lib/app.js';

const publicRoot = resolve('public');
const password = process.env.LOCAL_ADMIN_PASSWORD || 'local-demo-only';
const salt = randomBytes(16);
const deviceToken = process.env.LOCAL_DEVICE_TOKEN || 'local-device-token-0123456789abcdef';
const env = {
  ADMIN_PASSWORD_SCRYPT: `${salt.toString('hex')}:${scryptSync(password, salt, 64).toString('hex')}`,
  SESSION_SECRET: randomBytes(32).toString('hex'),
  DEVICE_TOKEN_SHA256: createHash('sha256').update(deviceToken).digest('hex'),
};
const values = new Map();
const store = {
  async get(key) { return values.get(key) || null; },
  async setJSON(key, value) { values.set(key, structuredClone(value)); },
  async delete(key) { values.delete(key); },
};

createServer(async (incoming, outgoing) => {
  try {
    const url = new URL(incoming.url, 'http://localhost:8787');
    if (url.pathname.startsWith('/api/')) {
      const chunks = [];
      for await (const chunk of incoming) chunks.push(chunk);
      const request = new Request(url, {
        method: incoming.method,
        headers: incoming.headers,
        body: chunks.length ? Buffer.concat(chunks) : undefined,
      });
      const response = await handleRequest(request, env, store);
      outgoing.writeHead(response.status, Object.fromEntries(response.headers));
      outgoing.end(Buffer.from(await response.arrayBuffer()));
      return;
    }
    const file = resolve(publicRoot, `.${url.pathname === '/' ? '/index.html' : url.pathname}`);
    if (!file.startsWith(publicRoot + sep)) { outgoing.writeHead(403).end(); return; }
    const content = await readFile(file);
    const type = file.endsWith('.css') ? 'text/css' : file.endsWith('.js') ? 'text/javascript' :
      file.endsWith('.svg') ? 'image/svg+xml' : 'text/html';
    outgoing.writeHead(200, { 'Content-Type': `${type}; charset=utf-8` });
    outgoing.end(content);
  } catch {
    outgoing.writeHead(404).end('Not found');
  }
}).listen(8787, '127.0.0.1', () => console.log('Local preview: http://localhost:8787 (password: local-demo-only)'));
