export type CatalogSource =
  | 'VITE_PACK_CATALOG_URL'
  | 'PACKS_REMOTE_MANIFEST'
  | 'PACK_CATALOG_URL'
  | 'fallback';

export interface CatalogResolution {
  url: string;
  source: CatalogSource;
}

function pickFirstNonEmpty(
  entries: Array<{ value: string | undefined | null; source: CatalogSource }>,
): CatalogResolution {
  for (const entry of entries) {
    const value = entry.value?.trim();
    if (value) {
      return { url: value, source: entry.source };
    }
  }

  return { url: '/packs.json', source: 'fallback' };
}

export function resolveCatalog(): CatalogResolution {
  const env = import.meta.env as Record<string, string | undefined>;
  return pickFirstNonEmpty([
    { value: env.VITE_PACK_CATALOG_URL, source: 'VITE_PACK_CATALOG_URL' },
    { value: env.PACKS_REMOTE_MANIFEST, source: 'PACKS_REMOTE_MANIFEST' },
    { value: env.PACK_CATALOG_URL, source: 'PACK_CATALOG_URL' },
  ]);
}

export function resolvePacksHost(): string {
  const env = import.meta.env as Record<string, string | undefined>;
  return env.VITE_PACKS_HOST?.trim() || 'https://packs.wordbird.app';
}
