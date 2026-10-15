# `v1.2.0`

**The overlay now requires [Ultrahand Overlay](https://github.com/ppkantorski/Ultrahand-Overlay) instead of Tesla Menu.** Extract Ultrahand's `sdout.zip` to the root of your SD card (it includes its own nx-ovlloader), then install emuiibo as usual. If you already have Ultrahand (for instance from another project that bundles it), there is no need to install it again.

## emuiibo

- Added a lightweight command that only reads a virtual amiibo's name, which the overlay now uses to list folders instead of fully loading every amiibo (areas, Mii, access ID cache). Amiibos are still fully loaded when selected.
- The access ID cache on the SD card is now only rewritten when an entry is new or changed, instead of every time an amiibo with saved game areas is loaded.
- The sysmodule is now built optimized for size, which reduces its always-resident memory by about 130KB.

## overlay

- Migrated from libtesla to [libultrahand](https://github.com/ppkantorski/libultrahand). The overlay follows your Ultrahand theme, wallpaper, footer language, and sound/haptic settings.
- The L/R (emulation off/on) and ZL/ZR (random UUID off/on) shortcuts now act on a quick tap of that button alone, on release. Opening or closing the overlay with Ultrahand's default combo (ZL + ZR + DPAD-down) no longer changes the active amiibo's random UUID setting.
- These shortcuts now also work when no list item is focused (for instance in an empty folder).
- Fixed the focused amiibo's image being decoded again on every frame, which slowed down the overlay while browsing.
- Reduced the overlay's memory usage by about 144KB: the buffer used to read game titles for the active amiibo's areas is now only allocated while reading them.
- Folders open much faster: amiibo names are now read with the new lightweight command, and saved to an index (in `sd:/emuiibo/overlay/cache`) the first time a folder is opened, so later visits skip reading each amiibo entirely. For instance, a folder of ~500 amiibos went from ~4 seconds to ~2 seconds on the first visit, and well under a tenth of a second afterwards. If an amiibo's name is edited in place, delete the `cache` folder to rebuild the indexes.
- Touch input follows Ultrahand's behavior (tapping a row focuses it, sliding off a row cancels the tap).
