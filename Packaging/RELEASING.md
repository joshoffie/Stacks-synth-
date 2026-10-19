# Shipping a build

Everything here runs on your Mac. Friends only ever receive the `.pkg` file.

## Each time you want to send a new build

1. **Bump the version.** Open `CMakeLists.txt` and change the number on this line:

   ```
   project(Stacks VERSION 0.1.0 LANGUAGES C CXX)
   ```

   0.1.0 → 0.1.1 → 0.1.2 and so on. macOS uses this number to decide that a
   package is an upgrade; a second build with the same number may be skipped
   as "already installed", and the friend would keep the old binaries.

2. **Build, sign and notarise.** From the project folder:

   ```bash
   Packaging/build-installer.sh --model ~/.ollama/models/blobs/sha256-3e4cb14174460404e7a233e531675303b2fbf7749c02f91864fe311ab6344e4f --notary-profile stacks
   ```

   This rebuilds the AU, VST3 and app in Release, signs them with the
   Developer ID Application certificate (hardened runtime), bundles the engine
   data, builds the package, signs it with the Developer ID Installer
   certificate, uploads it to Apple's notary service, waits for "Accepted"
   (a few minutes) and staples the ticket. It leaves
   `dist/Stacks-<version>.pkg` (about 2.3 GB).

   `--skip-build` reuses the binaries already in `build.nosync` (handy when only
   the package needs remaking).

3. **Send the file.** Upload `dist/Stacks-<version>.pkg` to Drive, Dropbox or
   similar and share the link.

## What the friend does

Download, double click, click through. Same steps for the first install and
every update; the new version replaces the old one. Then restart the DAW (or
in Logic: Plug-in Manager → Reset & Rescan). Stacks appears under Hoffman Audio.

Because the package is notarised and stapled, Gatekeeper opens it with a
double click; no right-click → Open, no "Open Anyway".

## What the parts mean

- `--model` is the engine data file (a GGUF) that gets bundled into the
  package and installed to `/Library/Application Support/Stacks/models`. The
  path above is Ollama's copy of Qwen3 4B on this Mac. Any
  `Qwen3-4B-Q4_K_M.gguf` works; the script also looks in
  `~/Library/Application Support/Stacks/models` when `--model` is omitted.
- `--notary-profile stacks` names the Apple credentials stored in the login
  keychain on 2026-10-19 with:

  ```bash
  xcrun notarytool store-credentials stacks --apple-id jhrunner1@gmail.com --team-id P6PE9BPQ2R
  ```

  It asks for an **app-specific password** (made at account.apple.com →
  Sign-In and Security → App-Specific Passwords), not the Apple ID password.
  Only needed again on a new Mac or after the password is revoked.
- Signing uses the "Developer ID Application" and "Developer ID Installer"
  certificates for team P6PE9BPQ2R, already in the login keychain. If they
  expire, renew them at developer.apple.com → Certificates and install the
  new ones; nothing in the script changes.

## Files

- `Packaging/build-installer.sh` — the whole pipeline.
- `Packaging/distribution.xml` — the installer's choices and text (no mention of what runs underneath).
- `Packaging/welcome.txt`, `conclusion.txt` — the screens the friend reads.
- `Packaging/Stacks.entitlements` — hardened-runtime entitlements for the bundles.
- `dist/` — output, ignored by git.
