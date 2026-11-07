# Building nRF52 applications on Windows ARM64

The [Windows setup in the README](../README.md#windows-native-powershell) targets x64. On Windows 11 ARM64, Zephyr SDK 1.0.1 has no native ARM64 host package, and `west sdk install` rejects the ARM64 host. An x64 Python environment and the Windows x64 SDK work under Windows emulation. This was verified in a Windows 11 ARM64 Parallels VM.

First install the host tools from the README, then close and reopen PowerShell. Install x64 Python 3.12 explicitly, even if native ARM64 Python is already installed:

```powershell
winget install --id Python.Python.3.12 --exact --architecture x64 --force
$pythonX64 = "$HOME\AppData\Local\Programs\Python\Python312\python.exe"
& $pythonX64 -m venv "$HOME\zephyrproject\.venv-x64"
$env:PATH = "$HOME\zephyrproject\.venv-x64\Scripts;$env:PATH"
python -m pip install west
west init -m https://github.com/zephyrproject-rtos/zephyr --mr v4.5.0 "$HOME\zephyrproject"
Set-Location "$HOME\zephyrproject"
west update
python -m pip install -r "$HOME\zephyrproject\zephyr\scripts\requirements-base.txt"
west zephyr-export
```

Download these two official files from the [Zephyr SDK 1.0.1 release](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v1.0.1) into `Downloads`:

- `zephyr-sdk-1.0.1_windows-x86_64_minimal.7z`
- `toolchain_gnu_windows-x86_64_arm-zephyr-eabi.7z`

Extract the minimal SDK, then the ARM GNU toolchain into its `gnu` directory. Register the SDK with CMake:

```powershell
7z x "$HOME\Downloads\zephyr-sdk-1.0.1_windows-x86_64_minimal.7z" "-o$HOME"
7z x "$HOME\Downloads\toolchain_gnu_windows-x86_64_arm-zephyr-eabi.7z" "-o$HOME\zephyr-sdk-1.0.1\gnu"
& "$HOME\zephyr-sdk-1.0.1\setup.cmd" /c
```

If `7z` is not on `PATH`, use `& 'C:\Program Files\7-Zip\7z.exe'` in place of `7z`. Confirm that `arm-zephyr-eabi-gcc.exe` is under `$HOME\zephyr-sdk-1.0.1\gnu\arm-zephyr-eabi\bin`.

Set these options in the installed BlitzMax SDK's `bin/custom.bmk`, replacing `you` with your Windows username:

```text
addoption nrf52.zephyr "C:/Users/you/zephyrproject/zephyr"
addoption nrf52.sdk "C:/Users/you/zephyr-sdk-1.0.1"
addoption nrf52.venv "C:/Users/you/zephyrproject/.venv-x64"
```

Build an example using the commands in the [README](../README.md#quick-start). This route builds UF2 images under x64 emulation; USB flashing from the VM was not tested. Keep the project path short because Zephyr's generated object paths can exceed Windows' path limit.
