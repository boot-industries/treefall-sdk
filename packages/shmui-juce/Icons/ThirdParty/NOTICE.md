# NOTICE — third-party artwork in the ShmUI console icon set

The console, transport and hotkey-legend icons shipped in
`juce/Source/Icons/` are derived from the artwork listed below. The generated
geometry in `juce/Source/Icons/ConsoleIconGeometry.cpp` is a uniform scaling and
translation of those files into the shared 24-unit design grid; no upstream
path data was re-drawn.

Machine-readable provenance for every mark — upstream file, immutable
revision, licence, local digest, geometry, normalisation transform and the
selection decision — is in `provenance.json` next to this file.

This directory is installed with the package
(`<install prefix>/share/shmui-juce/ThirdParty/`), including the upstream SVG
files, so that the recorded digests can be verified offline.

## Bootstrap Icons

* Copyright (c) 2019-2024 The Bootstrap Authors.
* Licence: MIT. The full licence text is in `bootstrap-icons/LICENSE`.
* Obligation: reproduce the copyright notice and the permission notice in
  distributed products. This NOTICE and `bootstrap-icons/LICENSE` satisfy that
  obligation for the ShmUI and Treefall SDK distributions.
* Repository: <https://github.com/twbs/icons>
* Revision used: `e568465481c0c541f63a78f6452d09387711d489`
* Files used: `icons/play-fill.svg`, `icons/pause-fill.svg`,
  `icons/stop-fill.svg`, `icons/record-fill.svg`, `icons/rewind-fill.svg`,
  `icons/fast-forward-fill.svg`, `icons/skip-start-fill.svg`,
  `icons/skip-end-fill.svg`, `icons/shift-fill.svg`

## Tabler Icons

* Copyright (c) 2020-2026 Paweł Kuna.
* Licence: MIT. The full licence text is in `tabler-icons/LICENSE`.
* Obligation: reproduce the copyright notice and the permission notice in
  distributed products. This NOTICE and `tabler-icons/LICENSE` satisfy that
  obligation for the ShmUI and Treefall SDK distributions.
* Repository: <https://github.com/tabler/tabler-icons>
* Revision used: `a4ce1404bc6d24d3c365afe7b258d6bf6f48d62d`
* File used: `icons/outline/repeat.svg`

## Bhutan stop sign

* Author: Fry1989 (Google Streetview); source file:
  `wikimedia-commons/Bhutan stop sign.svg`.
* Licence: public domain. No licence obligation attaches to the artwork; the
  credit is retained here as a courtesy.
* Description page: <https://commons.wikimedia.org/wiki/File:Bhutan_stop_sign.svg>
* Revision used: the file's own SHA-1 `8cfb6afd46f8575f8987551b928857c7164771ca`
  (SHA-256 `8f3e69d45addcc46…`, recorded in `provenance.json`).
* Use: the outer hexagon silhouette of the traffic stop sign, used as the
  Stop Peers badge.

## In-house artwork

The fade-in and fade-out wedges are the ShmUI/Orpheus design-kit geometry from
`apps/www/public/design-preview/components-clip-buttons.html`. They are covered
by this repository's own MIT licence; no third-party obligation applies. The
fade-out wedge is the design-kit fade-in wedge reflected about the design-kit
box centre, so that both bottom corners share one y-value; see
`provenance.json` for the recorded decision.
