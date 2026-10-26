# Working on this repo

Sin and Punishment Recompiled: one codebase for the PC (Windows/Linux) and Android releases.
Read README.md, BUILDING.md, STATUS.md, CHANGELOG.md, android/README.md and android/PLAN.md
before starting.

## Branches and releases

- PC work happens on `main`.
- The `android` branch is kept equal to `main`. Android-specific work goes there first, then
  merges into `main`.
- Releases are shared: each GitHub release (tag like `0.2.5`) carries the Windows zip, the
  Linux tar and the Android APK, all the same version.
- With each release, bump `versionName` (same as the tag) and `versionCode` (always higher)
  in `android/app/build.gradle.kts`, and add a CHANGELOG.md entry.

## How the owner builds

- PC: on Windows, in `C:\dev\snprecomp` (BUILDING.md). Packaging: `tools/package-windows.ps1`
  and `tools/package-linux.sh`.
- Android: a second checkout on the `android` branch, `C:\dev\snprecomp-android`, built
  with Android Studio's Run button.
- After any change to `snp.jp.toml`, the symbols or `patches/`, the owner rebuilds PC and
  copies `RecompiledFuncs/`, `RecompiledPatches/` and `rsp/` into the Android checkout. Say
  so when a change needs it.
- Release APK: `.\gradlew assembleRelease` in `android/`, signed with the owner's key
  (`keystore.properties`, never committed). Updates reach users through Obtainium.

## Rules

- Keep desktop and Android code separate: Android code behind `ANDROID` / `__ANDROID__`
  (or `CMAKE_CROSSCOMPILING`), so the desktop build is unaffected.
- Never commit the owner's real name, a keystore, passwords or any other key material.
- No ROM data or generated game code (`RecompiledFuncs/`, `RecompiledPatches/`, `rsp/`) in
  the repo.

## Open items

- Pause the game while the Android app is in the background (android/PLAN.md, step 4).
- Rice texture packs on Android: no way yet to put a pack in `texture_packs/` (e.g. an
  Install Pack button taking a .zip through the document picker).
- The screen filter on the HUD and crosshair only.
