import { defineConfig } from 'vite';

const packsCatalog =
  process.env.PACKS_REMOTE_MANIFEST || process.env.PACK_CATALOG_URL || '';
const packsHost = process.env.PACKS_HOST || 'https://packs.wordbird.app';

export default defineConfig({
  define: {
    'import.meta.env.VITE_PACK_CATALOG_URL': JSON.stringify(
      process.env.VITE_PACK_CATALOG_URL || packsCatalog,
    ),
    'import.meta.env.PACKS_REMOTE_MANIFEST': JSON.stringify(
      process.env.PACKS_REMOTE_MANIFEST || '',
    ),
    'import.meta.env.PACK_CATALOG_URL': JSON.stringify(process.env.PACK_CATALOG_URL || ''),
    'import.meta.env.VITE_PACKS_HOST': JSON.stringify(packsHost),
    'self.__WB_PACKS_HOST__': JSON.stringify(packsHost),
  },
});
