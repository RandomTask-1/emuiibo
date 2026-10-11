# `v1.1.5`

## emuiibo

- Fixed a potential crash when producing the in-game nickname of a virtual amiibo whose name contains non-ASCII characters.

## overlay

- Fixed the emuiibo version mismatch warning never being shown, since the status was always overwritten right after being set.

## emuiigen

- Virtual amiibo names are no longer stripped of spaces/punctuation and clamped to 10 characters, so the overlay now shows the full name (for instance `Link - Link's Awakening` instead of `LinkLinksA`). Games still only see the first 10 characters, which is the actual limit for an amiibo's in-game nickname.

- Generation now runs on a background thread, so the window keeps repainting instead of being reported as not responding for the duration of a batch. Progress is shown in the status bar, and failures are collected into a single report at the end rather than one dialog per amiibo.

### Previously released in `v1.1.4` (emuiigen only)

- Fixed amiibo images being saved in RGB format (the overlay requires RGBA).
- Oversized images are now downscaled, to prevent upscale errors in the overlay.
- Added WebP support for AmiiboAPI images served with a `.png` extension.
- Added an image resolution dropdown (128x128 / 256x256 / 512x512 / unmodified).
- Added batch grouping: amiibos can be split into alphabetical folders by a configurable group size (0 / 5 / 10 / 15 / 20), for better overlay performance.
