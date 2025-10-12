import type { Metadata } from "next"

import { OrpheusProvider } from "@orpheus/react"

import { META_THEME_COLORS, siteConfig } from "@/lib/config"
import { fontVariables } from "@/lib/fonts"
import { buildAbsoluteUrl, getMetadataBase, resolveAppUrl } from "@/lib/app-url"
import { cn } from "@/lib/utils"
import { LayoutProvider } from "@/hooks/use-layout"
import { ActiveThemeProvider } from "@/components/active-theme"
import { Analytics } from "@/components/analytics"
import { OrpheusDebugPanel } from "@/components/orpheus-debug-panel"
import { TailwindIndicator } from "@/components/tailwind-indicator"
import { ThemeProvider } from "@/components/theme-provider"
import { Toaster } from "@/registry/elevenlabs-ui/ui/sonner"

import "@/styles/globals.css"

const appUrl = resolveAppUrl()

export const metadata: Metadata = {
  title: {
    default: siteConfig.name,
    template: `%s | ${siteConfig.name}`,
  },
  metadataBase: getMetadataBase(),
  description: siteConfig.description,
  keywords: ["ElevenLabs", "UI", "shadcn", "Components", "audio", "agents"],
  authors: [
    {
      name: "ElevenLabs",
      url: "https://elevenlabs.io",
    },
  ],
  creator: "elevenlabs",
  openGraph: {
    type: "website",
    locale: "en_US",
    url: appUrl,
    title: siteConfig.name,
    description: siteConfig.description,
    siteName: siteConfig.name,
    images: [
      {
        url: buildAbsoluteUrl("/opengraph-image.png"),
        width: 1200,
        height: 630,
        alt: siteConfig.name,
      },
    ],
  },
  twitter: {
    card: "summary_large_image",
    title: siteConfig.name,
    description: siteConfig.description,
    images: [buildAbsoluteUrl("/opengraph-image.png")],
    creator: "@elevenlabsio",
  },
  icons: {
    icon: "/favicon.ico",
    shortcut: "/favicon-16x16.png",
    apple: "/apple-touch-icon.png",
  },
  manifest: buildAbsoluteUrl("/site.webmanifest"),
}

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode
}>) {
  return (
    <html lang="en" suppressHydrationWarning>
      <head>
        <script
          dangerouslySetInnerHTML={{
            __html: `
              try {
                if (localStorage.theme === 'dark' || ((!('theme' in localStorage) || localStorage.theme === 'system') && window.matchMedia('(prefers-color-scheme: dark)').matches)) {
                  document.querySelector('meta[name="theme-color"]').setAttribute('content', '${META_THEME_COLORS.dark}')
                }
                if (localStorage.layout) {
                  document.documentElement.classList.add('layout-' + localStorage.layout)
                }
              } catch (_) {}
            `,
          }}
        />
        <meta name="theme-color" content={META_THEME_COLORS.light} />
      </head>
      <body
        className={cn(
          "text-foreground group/body overscroll-none font-sans antialiased [--footer-height:calc(var(--spacing)*14)] [--header-height:calc(var(--spacing)*14)] xl:[--footer-height:calc(var(--spacing)*24)]",
          fontVariables
        )}
      >
        <ThemeProvider>
          <LayoutProvider>
            <ActiveThemeProvider>
              <OrpheusProvider config={{ autoConnect: true }}>
                {children}
                <TailwindIndicator />
                <Toaster position="top-center" />
                <Analytics />
                <OrpheusDebugPanel />
              </OrpheusProvider>
            </ActiveThemeProvider>
          </LayoutProvider>
        </ThemeProvider>
      </body>
    </html>
  )
}
