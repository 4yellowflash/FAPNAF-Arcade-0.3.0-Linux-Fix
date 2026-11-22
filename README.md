# proton-wintrust-fix

A lightweight, open-source `wintrust.dll` compatibility shim for **Wine / Proton / Steam Deck** that fixes immediate startup crashes in Unity IL2CPP games (such as **FAP N.A.F. ARCADE MODE**) caused by Authenticode signature checks on Wine system DLLs.

---

## The Problem

When launching affected games under Wine or Proton, the game immediately exits with an empty error dialog and a null-pointer crash (`0xc0000005` reading `0x4` in `GameAssembly.dll`).

**Why it happens:**
1. At startup, `GameAssembly.dll` enumerates loaded modules (`K32EnumProcessModules`) and checks that Windows system DLLs (starting with `C:\windows\system32\ntdll.dll`) carry a valid Microsoft Authenticode signature via `WinVerifyTrust`.
2. Wine/Proton's builtin DLLs in `C:\windows\system32\` are not signed by Microsoft and have no Windows signature catalog, so `WinVerifyTrust` returns `TRUST_E_NOSIGNATURE` (`0x800B0100`).
3. The game treats this as a failed integrity check (`anti_injection: detection_triggered`) and skips part of its initialization, causing an intentional crash before the window even opens.

## How This Fix Works

`wintrust.dll` is a small (~11 KB) standalone C shim (`wintrust_shim.c`):
- For files under `C:\windows\`, `WinVerifyTrust` returns `ERROR_SUCCESS` and provides an in-memory `CRYPT_PROVIDER_DATA` / `CRYPT_PROVIDER_SGNR` chain holding a self-signed certificate context (`CN=Microsoft Windows, O=Microsoft Corporation`) via `CertCreateSelfSignCertificate`.
- `CertGetNameStringW` in Wine's own `crypt32.dll` then succeeds natively.
- **No game files are modified.**

---

## Preparing 

Download the source code from the **Releases** page and extract it.

### Option 1: Building from Source

Run this command inside the project folder (Requires MinGW-w64 package installed on your system `x86_64-w64-mingw32-gcc`):

```bash
x86_64-w64-mingw32-gcc -O2 -shared -o wintrust.dll wintrust_shim.c wintrust.def \
  -nostartfiles -Wl,--entry,DllMain -lkernel32 -lcrypt32
```
### Option 2: Download pre-built (`wintrust.dll`)

Download pre-built (`wintrust.dll`) from **Releases** page into the project folder.

 > **Important:** Do **not** drop `wintrust.dll` into the game folder next to the `.exe`. Wine treats `wintrust.dll` as a `KnownDll` and will ignore copies outside `drive_c/windows/system32/`.

## Installation

### Option 1: Automatic installer (recommended)
Run `install.sh` with the path to your Wine or Proton prefix:
```bash
chmod +x install.sh
./install.sh /path/to/your/wine-or-proton-prefix
```
*(Works with both Lutris/Wine prefixes and Steam `compatdata/<appid>` folders. Automatically backs up the original `wintrust.dll` as `wintrust.dll.bak` and removes it from `tracked_files` so Proton updates don't overwrite it.)*

### Option 2: Manual installation
1. Open your Wine/Proton prefix and navigate to:
   ```
   <prefix>/drive_c/windows/system32/
   ```
2. Rename, backup or delete the existing `wintrust.dll` (usually a symlink to Proton's copy).
3. Copy `wintrust.dll` from this release into `<prefix>/drive_c/windows/system32/`.

### Step 2: Enable the DLL override
Set `WINEDLLOVERRIDES="wintrust=n"` in your launcher:

- **Steam / Steam Deck:** Right-click game → **Properties → Launch Options**:
  ```
  WINEDLLOVERRIDES="wintrust=n" %command%
  ```
- **Lutris:** Right-click game → **Configure → Runner options → DLL overrides**:
  - Key: `wintrust`
  - Value: `n`
- **Heroic:** Game **Settings → Advanced → Environment Variables**:
  - Variable: `WINEDLLOVERRIDES`
  - Value: `wintrust=n`
- **Bottles:** Open your bottle → **Settings → DLL Overrides**:
  - New Override: `wintrust`
  - Set to: `Native, then Builtin`
---

## Uninstall
Delete `<prefix>/drive_c/windows/system32/wintrust.dll`, rename `wintrust.dll.bak` back to `wintrust.dll`, and remove the `WINEDLLOVERRIDES` option.

---

Tested on **GE-Proton 11-12** (Linux x86_64).

---

## Credits
- **[exys](https://github.com/4yellowflash)** — testing, diagnostics, and repository maintenance
- **Antigravity (Google DeepMind)** — Wine relay trace analysis and `wintrust.dll` shim implementation

---

## Disclaimer
This project contains no game files or proprietary Windows binaries. It is solely a Wine/Proton compatibility shim to allow legally owned Windows games to run on Linux. Licensed under the [MIT License](LICENSE).
