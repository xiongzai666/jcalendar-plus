import { createHash, createHmac, scryptSync, timingSafeEqual } from 'node:crypto';

const SESSION_SECONDS = 7 * 24 * 60 * 60;

function sameBytes(left, right) {
  const a = Buffer.from(left);
  const b = Buffer.from(right);
  return a.length === b.length && timingSafeEqual(a, b);
}

export function hasSecrets(env) {
  return Boolean(env.ADMIN_PASSWORD_SCRYPT && env.SESSION_SECRET && env.DEVICE_TOKEN_SHA256);
}

export function verifyPassword(password, encoded) {
  if (typeof password !== 'string' || password.length > 256 || typeof encoded !== 'string') return false;
  const [salt, expected] = encoded.split(':');
  if (!/^[0-9a-f]{32}$/.test(salt || '') || !/^[0-9a-f]{128}$/.test(expected || '')) return false;
  const actual = scryptSync(password, Buffer.from(salt, 'hex'), 64);
  return sameBytes(actual, Buffer.from(expected, 'hex'));
}

export function createSession(secret, issuedAt = Date.now()) {
  const payload = Buffer.from(JSON.stringify({ exp: Math.floor(issuedAt / 1000) + SESSION_SECONDS }))
    .toString('base64url');
  const signature = createHmac('sha256', secret).update(payload).digest('base64url');
  return `${payload}.${signature}`;
}

export function validSession(request, secret, now = Date.now()) {
  if (!secret) return false;
  const cookie = request.headers.get('cookie') || '';
  const token = cookie.split(';').map(part => part.trim()).find(part => part.startsWith('jc_session='))?.slice(11);
  if (!token || token.length > 256) return false;
  const [payload, signature, extra] = token.split('.');
  if (!payload || !signature || extra) return false;
  const expected = createHmac('sha256', secret).update(payload).digest('base64url');
  if (!sameBytes(signature, expected)) return false;
  try {
    const data = JSON.parse(Buffer.from(payload, 'base64url').toString('utf8'));
    return Number.isSafeInteger(data.exp) && data.exp > Math.floor(now / 1000);
  } catch {
    return false;
  }
}

export function validDevice(request, expectedHash) {
  if (!/^[0-9a-f]{64}$/.test(expectedHash || '')) return false;
  const authorization = request.headers.get('authorization') || '';
  const token = /^Bearer ([A-Za-z0-9_-]{32,128})$/.exec(authorization)?.[1];
  if (!token) return false;
  const actual = createHash('sha256').update(token).digest('hex');
  return sameBytes(actual, expectedHash);
}

export const SESSION_COOKIE = 'Path=/api; HttpOnly; Secure; SameSite=Strict; Max-Age=604800';
