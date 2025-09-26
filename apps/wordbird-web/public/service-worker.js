const PACKS_CACHE = 'wordbird-packs-v1';
const rawPacksHost = self.__WB_PACKS_HOST__ || 'https://packs.wordbird.app';
let packsOrigin;
try {
  packsOrigin = new URL(rawPacksHost).origin;
} catch (error) {
  console.error('Wordbird SW: invalid packs host', rawPacksHost, error);
  packsOrigin = 'https://packs.wordbird.app';
}

self.addEventListener('install', (event) => {
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(self.clients.claim());
});

self.addEventListener('fetch', (event) => {
  if (event.request.method !== 'GET') {
    return;
  }

  const url = new URL(event.request.url);
  const isLocalPack =
    url.origin === self.location.origin &&
    url.pathname.startsWith('/packs/') &&
    url.pathname.endsWith('.sqlite.zst');
  const isCdnPack = url.origin === packsOrigin && url.pathname.endsWith('.sqlite.zst');

  if (!(isLocalPack || isCdnPack)) {
    return;
  }

  event.respondWith(
    caches.open(PACKS_CACHE).then(async (cache) => {
      const cached = await cache.match(event.request, { ignoreSearch: true });
      if (cached) {
        return cached;
      }

      const response = await fetch(event.request);
      if (response && response.ok) {
        void cache.put(event.request, response.clone());
      }
      return response;
    }),
  );
});
