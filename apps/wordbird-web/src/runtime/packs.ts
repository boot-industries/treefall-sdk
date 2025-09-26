import { resolveCatalog, resolvePacksHost } from './config';

type CatalogEntry = {
  id: string;
  sha256: string;
  url?: string;
};

type Catalog = {
  packs: CatalogEntry[];
};

async function sha256(buffer: ArrayBuffer): Promise<string> {
  const digest = await crypto.subtle.digest('SHA-256', buffer);
  const bytes = Array.from(new Uint8Array(digest));
  return bytes.map((b) => b.toString(16).padStart(2, '0')).join('');
}

function readMagic(bytes: Uint8Array): string {
  return bytes
    .slice(0, 4)
    .map((value) => value.toString(16).padStart(2, '0'))
    .join(' ');
}

function ensureZstdMagic(bytes: Uint8Array): void {
  const MAGIC = [0x28, 0xb5, 0x2f, 0xfd];
  for (let i = 0; i < MAGIC.length; i += 1) {
    if (bytes[i] !== MAGIC[i]) {
      const magic = readMagic(bytes);
      throw new Error(`pack payload invalid (no zstd magic; got ${magic})`);
    }
  }
}

export interface BootstrapOutcome {
  catalogUrl: string;
  catalogSource: string;
  packsHost: string;
  catalog: Catalog | null;
}

async function fetchJson(url: string): Promise<any> {
  const response = await fetch(url, {
    credentials: 'omit',
  });
  if (!response.ok) {
    throw new Error(`failed to download pack catalog (${response.status})`);
  }
  return response.json();
}

function resolvePackUrl(entry: CatalogEntry, packsHost: string): string {
  if (entry.url?.startsWith('http')) {
    return entry.url;
  }

  const base = new URL(packsHost);
  const path = entry.url ?? `/packs/${entry.id}.sqlite.zst`;
  return new URL(path, base).toString();
}

async function downloadPack(entry: CatalogEntry, packsHost: string): Promise<void> {
  const packUrl = resolvePackUrl(entry, packsHost);
  console.info('Wordbird: downloading pack', { id: entry.id, url: packUrl });
  const response = await fetch(packUrl, {
    credentials: 'omit',
    redirect: 'follow',
  });

  console.info('Wordbird: pack response', {
    status: response.status,
    contentType: response.headers.get('content-type'),
    contentEncoding: response.headers.get('content-encoding'),
    contentLength: response.headers.get('content-length'),
    finalUrl: response.url,
  });

  if (!response.ok) {
    throw new Error(`pack download failed (${response.status})`);
  }

  const arrayBuffer = await response.arrayBuffer();
  const bytes = new Uint8Array(arrayBuffer);

  if (bytes.byteLength < 10_000) {
    console.warn('Wordbird: suspiciously small pack', {
      url: packUrl,
      bytes: bytes.byteLength,
    });
  }

  ensureZstdMagic(bytes);

  if (!entry.sha256) {
    throw new Error(`pack catalog missing sha256 for ${entry.id}`);
  }

  const digest = await sha256(arrayBuffer);
  if (digest !== entry.sha256.toLowerCase()) {
    throw new Error(
      `pack sha256 mismatch (expected ${entry.sha256.toLowerCase()} got ${digest})`,
    );
  }

  console.info('Wordbird: pack verified', { id: entry.id, bytes: bytes.byteLength });
}

export async function bootstrapFromCatalog(): Promise<BootstrapOutcome> {
  const { url: catalogUrl, source } = resolveCatalog();
  const candidateLocation =
    typeof window !== 'undefined' && window.location
      ? window.location
      : (globalThis as typeof globalThis & { location?: Location }).location;
  const baseOrigin = candidateLocation?.origin ?? 'https://wordbird.local';
  const resolvedUrl = catalogUrl.startsWith('http')
    ? catalogUrl
    : new URL(catalogUrl, baseOrigin).toString();
  const packsHost = resolvePacksHost();

  console.info('Wordbird: pack catalog', {
    resolved: resolvedUrl,
    source,
  });
  if (source === 'fallback') {
    console.warn('Wordbird: using fallback catalog URL (/packs.json)');
  }

  const catalog = (await fetchJson(resolvedUrl)) as Catalog;
  const primary = catalog.packs?.[0];
  if (primary) {
    await downloadPack(primary, packsHost);
  }

  return {
    catalogUrl: resolvedUrl,
    catalogSource: source,
    packsHost,
    catalog,
  };
}
