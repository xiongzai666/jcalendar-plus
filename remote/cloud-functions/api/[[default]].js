import { getStore } from '@edgeone/pages-blob';
import { handleRequest } from '../lib/app.js';

export async function onRequest(context) {
  const store = getStore({ name: 'jcalendar', consistency: 'strong' });
  return handleRequest(context.request, context.env, store);
}
