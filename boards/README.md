# nRF52 board profiles

Each directory contains a `board.ini` read by `bmk` when selecting `-board`.
The three required keys are:

| Key | Meaning |
| --- | --- |
| `zephyr_board` | Zephyr board name and qualifiers used for compilation |
| `uf2_board_id` | UF2 bootloader `Board-ID` required for `-x` upload |
| `overlay` | Pin route overlay in `cmake/application` |

Inspect `INFO_UF2.TXT` on the mounted UF2 drive before using a profile on a
new hardware revision. A plain XIAO tested with its factory 0.6.1 bootloader
reported `Seeed_XIAO_nRF52840_Sense`; the current Adafruit bootloader source
uses `nRF52840-SeeedXiao-v1` for the plain model. The Board-ID identifies the
installed bootloader, not the sensor hardware. Plus models currently share
Zephyr board definitions and pin routes with their non-Plus counterparts;
their extra pads remain available through SoC pin numbers.
