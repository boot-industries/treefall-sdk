import { siteConfig } from "@/lib/config"

const DEFAULT_APP_URL = "http://localhost:4000"

function normalizeUrl(value?: string | null): string | undefined {
  if (!value) {
    return undefined
  }

  try {
    return new URL(value).toString()
  } catch {
    return undefined
  }
}

export function resolveAppUrl(): string {
  return (
    normalizeUrl(process.env.NEXT_PUBLIC_APP_URL) ??
    normalizeUrl(process.env.NEXT_PUBLIC_SITE_URL) ??
    normalizeUrl(process.env.NEXT_PUBLIC_BASE_URL) ??
    normalizeUrl(siteConfig.url) ??
    DEFAULT_APP_URL
  )
}

export function getMetadataBase(): URL {
  const base = resolveAppUrl()

  try {
    return new URL(base)
  } catch {
    return new URL(DEFAULT_APP_URL)
  }
}

export function buildAbsoluteUrl(path: string): string {
  const base = resolveAppUrl()

  try {
    return new URL(path, base).toString()
  } catch {
    if (/^https?:\/\//.test(path)) {
      return path
    }

    const normalizedBase = base.endsWith("/") ? base.slice(0, -1) : base
    const normalizedPath = path.startsWith("/") ? path : `/${path}`

    return `${normalizedBase}${normalizedPath}`
  }
}
