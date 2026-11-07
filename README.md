# nrf52.mod

Zephyr platform support for BlitzMax NG applications on nRF52840 boards. The target provides implementations of the portable `Embedded.*` APIs and a few `NRF52.*` modules for behavior specific to this chip.

## Supported board

| Board | `-board` value | Bootloader |
| --- | --- | --- |
| Tenstar Robot ProMicro nRF52840 | `promicro_nrf52840` | nice!nano UF2, S140 v6.1.1 |

The Zephyr board qualifier is `promicro_nrf52840/nrf52840/uf2`. The target has been tested on the board above; other nRF52 variants are not yet supported by this adapter. The application is linked inside Zephyr's app partition.

## Requirements

- A BlitzMax NG installation containing matching nRF52-enabled `bcc2`, `bmk2`, `blitzmax.mod`, `brl.mod`, `embedded.mod`, and `nrf52.mod` revisions.
- Zephyr **4.5.0** and its west workspace, including the modules fetched by `west update`.
- Zephyr SDK **1.0.1** with the ARM toolchain.
- Host tools including CMake, Ninja, Python, and the devicetree compiler.
- A data-capable USB cable and a compatible UF2 bootloader for upload.

The [platform setup](#platform-setup) below installs the toolchain once. Later application builds use `bmk`.

## Quick start

Create `hello.bmx`:

```blitzmax
SuperStrict

Framework BRL.StandardIO

Print "Hello from BlitzMax on nRF52840"
```

With the board in `NICENANO` UF2 mode, build and upload it:

```sh
bmk makeapp -r -l nrf52 -g arm -board promicro_nrf52840 -x hello.bmx
```

On the Tenstar board, briefly bridge RST to GND twice to enter UF2 mode. Use the same command without `-x` to build only. The outputs are `.uf2`, `.elf`, and `.hex` files. `-x` uses Zephyr's UF2 runner with board ID `nRF52840-nicenano`; it requires the UF2 drive to be present already. `BRL.StandardIO` prints to the USB CDC serial port after USB enumeration.

For a broader runtime example, see [`nrf52_hello.bmx`](examples/nrf52_hello.bmx). Inspect the board or connected USB mode with:

```sh
bmk boardinfo -l nrf52 -board promicro_nrf52840
bmk deviceinfo -l nrf52
```

## Build options

| Option | Meaning |
| --- | --- |
| `-l nrf52` | Select the nRF52 target |
| `-g arm` | Select the ARM compiler architecture |
| `-board promicro_nrf52840` | Select the supported board profile |
| `-r` | Build optimised release firmware |
| `-d` | Include source-level debugging information |
| `-o <name>` | Choose the output name |
| `-x` | Upload the UF2 when the board is already in `NICENANO` mode |

The managed arena is currently fixed at 64 KiB and the Zephyr main stack at 16 KiB. These are build settings, not `-heap` options for this target. A managed allocation may fail before physical RAM is full.

## Tool configuration

Set the Zephyr source and SDK paths in the installed BlitzMax SDK's `bin/custom.bmk`:

```bmk
addoption nrf52.zephyr "/path/to/zephyrproject/zephyr"
addoption nrf52.sdk "/path/to/zephyr-sdk-1.0.1"
# Optional: additional devicetree overlay for a custom pin route
# addoption nrf52.overlay "/absolute/path/to/pins.overlay"
```

| `custom.bmk` option | Environment equivalent | Purpose |
| --- | --- | --- |
| `nrf52.zephyr` | `ZEPHYR_BASE` | Zephyr source directory |
| `nrf52.sdk` | `ZEPHYR_SDK_INSTALL_DIR` | Zephyr SDK root |
| `nrf52.venv` | `NRF52_VENV` | Workspace Python environment, when not beside the Zephyr source |
| `nrf52.west` | `WEST` | `west` executable or its containing directory |
| `nrf52.overlay` | `BMX_NRF52_EXTRA_OVERLAY_FILE` | Additional devicetree overlay applied after the built-in board overlay |

`custom.bmk` takes precedence over environment variables. `bmk` automatically finds `west` and Python packages in the workspace's sibling `.venv`, so you do not need to activate it for each build. Host tools still need to be on `PATH`.

## Platform setup

The commands below create a Zephyr 4.5.0 west workspace and install Zephyr SDK 1.0.1. Zephyr supplies the source and board definitions; the separate SDK supplies the ARM compiler. They follow the [Zephyr getting started guide](https://docs.zephyrproject.org/latest/develop/getting_started/).

### macOS (Apple silicon)

Install [Homebrew](https://brew.sh/) if needed, then run in Terminal:

```sh
brew install cmake ninja gperf python@3.12 python-tk ccache qemu dtc libmagic wget openocd
python3.12 -m venv "$HOME/zephyrproject/.venv"
source "$HOME/zephyrproject/.venv/bin/activate"
python -m pip install west
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.5.0 "$HOME/zephyrproject"
cd "$HOME/zephyrproject"
west update
west packages pip --install
west zephyr-export
cd zephyr
west sdk install -d "$HOME/zephyr-sdk-1.0.1" -t arm-zephyr-eabi
```

Add these lines to the installed BlitzMax SDK's `bin/custom.bmk`, replacing `you` with your macOS username:

```text
addoption nrf52.zephyr "/Users/you/zephyrproject/zephyr"
addoption nrf52.sdk "/Users/you/zephyr-sdk-1.0.1"
```

### Linux (Ubuntu 24.04 or newer)

Install the host tools, then create the pinned Zephyr workspace and ARM SDK:

```sh
sudo apt update
sudo apt install --no-install-recommends git cmake ninja-build gperf ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev
python3 -m venv "$HOME/zephyrproject/.venv"
source "$HOME/zephyrproject/.venv/bin/activate"
python -m pip install west
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.5.0 "$HOME/zephyrproject"
cd "$HOME/zephyrproject"
west update
west packages pip --install
west zephyr-export
cd zephyr
west sdk install -d "$HOME/zephyr-sdk-1.0.1" -t arm-zephyr-eabi
```

This package list follows Zephyr's Ubuntu setup. On ARM64 Linux, omit `gcc-multilib` and `g++-multilib`; the `file` package installs the appropriate `libmagic` library for the Ubuntu release. For other distributions, use [Zephyr's Linux host-dependency guide](https://docs.zephyrproject.org/latest/develop/getting_started/installation_linux.html). In the installed BlitzMax SDK's `bin/custom.bmk`, replace `you` with your Linux username:

```text
addoption nrf52.zephyr "/home/you/zephyrproject/zephyr"
addoption nrf52.sdk "/home/you/zephyr-sdk-1.0.1"
```

### Windows (native PowerShell)

In PowerShell, install the host tools with `winget`, then **close and reopen PowerShell** so the new commands are on `PATH`:

```powershell
$packages = 'Kitware.CMake', 'Ninja-build.Ninja', 'oss-winget.gperf', 'Python.Python.3.12', 'Git.Git', 'oss-winget.dtc', '7zip.7zip'
foreach ($package in $packages) { winget install --id $package --exact }
```

Create a Python virtual environment, fetch the pinned Zephyr workspace and its modules, then install the ARM SDK. Adding the virtual environment to this PowerShell session's `PATH` avoids changing PowerShell's script execution policy:

```powershell
py -3.12 -m venv "$HOME\zephyrproject\.venv"
$env:PATH = "$HOME\zephyrproject\.venv\Scripts;$env:PATH"
python -m pip install west
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.5.0 "$HOME\zephyrproject"
Set-Location "$HOME\zephyrproject"
west update
python -m pip install -r "$HOME\zephyrproject\zephyr\scripts\requirements-base.txt"
west zephyr-export
Set-Location zephyr
west sdk install -d "$HOME\zephyr-sdk-1.0.1" -t arm-zephyr-eabi
```

If `7z` is not found, add `C:\Program Files\7-Zip` (or its actual installed directory) to `PATH` as described in [Zephyr's Windows setup](https://docs.zephyrproject.org/latest/develop/getting_started/). In the installed BlitzMax SDK's `bin/custom.bmk`, use forward slashes in the paths and replace `you` with your Windows username:

```text
addoption nrf52.zephyr "C:/Users/you/zephyrproject/zephyr"
addoption nrf52.sdk "C:/Users/you/zephyr-sdk-1.0.1"
```

Windows on ARM64 needs [additional setup](docs/windows-arm64.md) to run the x64 Python packages and Zephyr SDK under emulation.
Keep Windows project paths short: Zephyr's generated object paths can exceed the Windows path limit in deeply nested folders.

### IDE-assisted installation

Zephyr-focused VS Code extensions such as [IDE for Zephyr](https://docs.zephyrproject.org/latest/develop/tools/ide_for_zephyr_vscode_ext.html) and [Workbench for Zephyr](https://docs.zephyrproject.org/latest/develop/tools/workbench_for_zephyr.html) can install host tools and the Zephyr SDK, create a west workspace, and manage builds. IDE for Zephyr's [Host Tools](https://zephyr-ide.mylonics.com/getting-started/host-tools/) panel detects missing build tools and offers installation through the host platform's package manager: Homebrew on macOS, winget on Windows, and the appropriate supported package manager on Linux. Use its SDK 1.0.1 installation if present; point `nrf52.sdk` at the actual SDK directory (for example, `$HOME/.zephyr_ide/toolchains/zephyr-sdk-1.0.1` on macOS).

Check the Zephyr version separately. If the IDE's source-version picker does not offer 4.5.0, use the platform commands above to create the pinned 4.5.0 west workspace while reusing the IDE-installed SDK; the `west sdk install` step can be skipped. [IDE for Zephyr can register an external west workspace](https://zephyr-ide.mylonics.com/getting-started/workspace-configuration/) if you also want to use that workspace in VS Code. Set `nrf52.zephyr` to its `zephyr` directory. BlitzMax applications still build through `bmk`.

## Portable and target-specific APIs

Use `Embedded.*` for code that should also run on ESP32 and Pico. The nRF52 target implements the portable GPIO, ADC, PWM, SPI, I²C, UART, random, time, power, and BLE APIs. Use `NRF52.*` for behavior specific to this chip, including System OFF, device identity, reset cause, and the BLE adapter.

For example, a GPIO application imports `Embedded.Hardware.GPIO`; a BLE application imports `NRF52.Network.BLE`. Event-driven features use `BRL.EventQueue` and require regular `PollEvent()` calls.

**BLE is opt-in.** Importing `NRF52.Network.BLE` adds the BLE source and automatically selects [`ble.conf`](cmake/application/ble.conf), enabling Zephyr Bluetooth, GATT, and pairing. An application without that import leaves Zephyr Bluetooth disabled.

**Thread is opt-in.** Importing `NRF52.Network.Thread` selects
[`thread.conf`](cmake/application/thread.conf) and Zephyr's OpenThread stack
for the nRF52840 IEEE 802.15.4 radio. The portable API and a two-board UDP
example are described in `embedded.mod/network.mod/thread.mod/README.md`; the local example
is [`nrf52_thread_echo.bmx`](examples/nrf52_thread_echo.bmx). Zephyr 4.5.0 must
include the OpenThread module fetched by `west update`. An external Thread
border router is required for off-mesh IPv6 access. The example's dataset
printout includes the network key and is intended only for local testing.

## Feature guide

| Area | Primary modules | Start with |
| --- | --- | --- |
| USB console and input | `BRL.StandardIO`, `NRF52.IO.StandardIO` | [`nrf52_usb_input.bmx`](examples/nrf52_usb_input.bmx) |
| Time, identity, random | `NRF52.System.Time`, `NRF52.System.Device`, `Embedded.Random` | [`nrf52_device.bmx`](examples/nrf52_device.bmx), [`nrf52_random.bmx`](examples/nrf52_random.bmx) |
| GPIO and interrupts | `Embedded.Hardware.GPIO`, `Embedded.Hardware.GPIOEvents` | [`nrf52_gpio.bmx`](examples/nrf52_gpio.bmx), [`nrf52_gpio_events.bmx`](examples/nrf52_gpio_events.bmx) |
| ADC and PWM | `Embedded.Hardware.ADC`, `Embedded.Hardware.PWM` | [`nrf52_adc.bmx`](examples/nrf52_adc.bmx), [`nrf52_pwm.bmx`](examples/nrf52_pwm.bmx) |
| SPI and I²C | `Embedded.Hardware.SPI`, `Embedded.Hardware.I2C` | [`nrf52_spi.bmx`](examples/nrf52_spi.bmx), [`nrf52_i2c.bmx`](examples/nrf52_i2c.bmx) |
| UART | `Embedded.Hardware.UART`, `Embedded.IO.BufferedUART` | [`nrf52_uart.bmx`](examples/nrf52_uart.bmx), [`nrf52_buffered_uart.bmx`](examples/nrf52_buffered_uart.bmx) |
| Watchdog and power | `NRF52.Hardware.Watchdog`, `Embedded.System.Power`, `NRF52.System.Power` | [`nrf52_watchdog.bmx`](examples/nrf52_watchdog.bmx), [`nrf52_system_off.bmx`](examples/nrf52_system_off.bmx) |
| BLE peripheral and central | `NRF52.Network.BLE`, `Embedded.Network.BLE` | [`nrf52_ble_gatt.bmx`](examples/nrf52_ble_gatt.bmx), [`nrf52_ble_gatt_client.bmx`](examples/nrf52_ble_gatt_client.bmx) |

## Board settings and limits

| Feature | Current board setting or limit |
| --- | --- |
| Pin numbers | P0.00–P0.31 are 0–31; P1.00–P1.15 are 32–47. Confirm the carrier's pad labels before wiring. Reset pin 18 and USB pins 24–25 are reserved. |
| ADC and PWM | ADC accepts P0.02–P0.05 and P0.28–P0.31. The [reference example](examples/nrf52_adc_reference.bmx) reports averaged raw and nominal millivolt readings for a measured voltage on D19/P0.02. PWM defaults to the LED at P0.15 and D4/P0.22. The [extra PWM overlay](examples/nrf52_pwm_extra.overlay) also enables D6/P1.00 and D7/P0.11, each on its own controller. Avoid simultaneous claims on one pin. |
| SPI | Controller 0 defaults to SCK P1.01, MOSI P1.02, MISO P1.07. A build overlay can change these pins; `SPIDefaultClockPin()`, `SPIDefaultTXPin()`, and `SPIDefaultRXPin()` then report the selected route. Control chip select through GPIO. |
| UART | Controller 0 defaults to TX D10/P0.09, RX D16/P0.10, RTS D3/P0.06, and CTS D2/P0.08. A build overlay can change these pins; pass the selected TX/RX pair to `UARTConfigurePins` and, if needed, both RTS/CTS pins to `UARTConfigureFlowControlPins`. `UARTSetBreak` holds TX low and temporarily pauses receive; it is unavailable while a buffered stream is open. Buffered UART needs `BRL.EventQueue`; do not mix buffered and polling access. |
| I²C | Controller 0 defaults to SDA D8/P1.04 and SCL D9/P1.06. A build overlay can change these pins; `I2CDefaultSDAPin()` and `I2CDefaultSCLPin()` report the selected route. Internal pull-ups are enabled by default; configure with `pullUps=True`. The [external pull-up overlay](examples/nrf52_i2c_external_pullups.overlay) disables them; use two resistors to 3.3 V and configure with `pullUps=False`. Blocking calls retain a 20 ms limit per message. Timed calls use the requested deadline for the complete transfer, including repeated Start. |
| Watchdog | Once started, it cannot be disabled in software; a power cycle stops it. |
| System OFF | `SystemOff()` and `SystemOffUntilGPIO()` stop the program. Wake restarts through the bootloader; save state before entering. There is no timer wake in System OFF. |
| BLE | One connection, four local GATT services, eight local characteristics, and four stored bonds in the current configuration. |

Advanced Zephyr Kconfig changes can be supplied through `BMX_NRF52_EXTRA_CONF_FILE`, set to the absolute path of an additional `.conf` file before building. The BLE fragment is still added automatically when the BLE module is imported.

To build with a different peripheral route, set `BMX_NRF52_EXTRA_OVERLAY_FILE` to an absolute `.overlay` path, or set `nrf52.overlay` in `custom.bmk`. Sample overlays are provided for [SPI](examples/nrf52_spi_alt.overlay), [I²C](examples/nrf52_i2c_alt.overlay), and [UART](examples/nrf52_uart_alt.overlay). Each overlay changes both the default and sleep pinctrl states. The build checks pin functions, distinct pins, reserved reset and USB pins, and conflicts with other configured peripherals. Verify the carrier's pad labels before wiring it. Pin changes require a rebuild and upload.

On a nice!nano v2 layout, the SPI sample selects SCK D6, MOSI D15, and MISO D14; connect D15 to D14 for the [`nrf52_spi.bmx`](examples/nrf52_spi.bmx) loopback check. The I²C sample selects SDA D7 and SCL D19; the [`nrf52_i2c.bmx`](examples/nrf52_i2c.bmx) example reads these pins from the build. The UART sample selects TX D6 and RX D7; connect those pads for the [`nrf52_uart_alt.bmx`](examples/nrf52_uart_alt.bmx) loopback check. UART also retains RTS D3 and CTS D2 in that sample.

The extra PWM overlay selects D6 and D7 for PWM2 and PWM3. Build it separately from the sample SPI, I²C, and UART route overlays, which also use those pads. The [`nrf52_pwm_extra.bmx`](examples/nrf52_pwm_extra.bmx) example checks both outputs in turn with D6 connected to D7.

## Troubleshooting

- If `bmk` cannot find Zephyr, the SDK, or `west`, check the [tool configuration](#tool-configuration) paths and that the west workspace has completed `west update`.
- If upload cannot find `NICENANO`, enter UF2 mode just before using `-x`. Building without `-x` does not require the board.
- If serial output is absent, check the data-capable USB connection and allow the USB CDC port to appear after the application starts.
- If a BLE example receives no events, import `BRL.EventQueue` and keep calling `PollEvent()`.

## Further documentation

The [board bring-up notes](docs/bringup-notes.md) contain detailed measured results, feature limits, and remaining peripheral work. The [examples](examples/) show each API in a complete application.
