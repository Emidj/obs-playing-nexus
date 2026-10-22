# OBS Playing NEXUS 1.0.0

Native OBS Studio plugin and Windows x64 DJ Companion for displaying now-playing information. Companion sends track text and artwork to the streaming PC over TLS. **1.0.0 is the author's official stable release.**

Includes 23 original designs. Compatibility depends on the version and state of VirtualDJ, rekordbox, Serato or Traktor; track information does not measure the audible output of an external mixer.

## What's new in 1.0.0

- Traktor Pro 3 / 4 integration in OBS and Companion: title, artist and artwork, with runtime structure validation. Real tests cover 3.5.0 build 270 and 4.4.1 build 22.
- Compact 440 × 640 Companion interface, with scrolling and accessible settings/connection controls in short windows.
- The dashboard lists only current authenticated connections. Saved PCs remain in settings; selection and revocation use device IDs.
- Five built-in languages, one properties panel and optional Twitch announcements, disabled by default.
- Existing identities, scenes, profiles, presets and all 23 original designs are preserved.

Traktor MASTER is the tempo master, not a measurement of audible output. The 3.x/4.x families require structure validation; not every version has been tested. The Traktor logo retains its third-party attribution.

## Preview

![OBS Playing NEXUS promotional preview](docs/images/obs-playing-nexus-preview.png)

*Promotional illustration of a now-playing overlay in OBS.*

<img src="docs/images/companion-demo.png" alt="OBS Playing Companion 1.0.0 compact interface" width="440">

*Actual Companion 1.0.0 compact interface, idle and disconnected.*

## Installation

Download the [official 1.0.0 installer](https://github.com/Emidj/obs-playing-nexus/releases/tag/v1.0.0). Close OBS and Companion before updating. Choose the OBS plugin on the streaming PC and Companion on the DJ PC. Installation into Program Files may request administrator permission. Update both components to 1.0.0 for artwork transfer between PCs.

Read the [English illustrated guide](playing-next/docs/companion/README.en.md) or the [Spanish guide](playing-next/docs/companion/README.md). The installer includes uninstallation and preserves backups of replaced versions.

The installer is unsigned. Compare its SHA256 with the release manifest.

## Source and build

See [REBUILD.txt](REBUILD.txt), [technical documentation](playing-next/README.md) and [publication audit](PUBLICATION-AUDIT.md). Building requires Windows x64, Visual Studio 2022 C++, CMake 3.28 or later and compatible Qt/OBS SDKs. Existing recipes contain paths from the original development environment; a clean Windows build has not been verified.

The corresponding 1.0.0 sources include the shared engine in `shared/`, Qt sources, recipes, patches and SBOM. Original Qt archives are distributed in the complete Sources ZIP alongside the installer; they must remain in the release even though they are excluded from Git history.

## Licenses

This project contains components under several licenses. [LICENSE](LICENSE) preserves the MIT license for the author's code; **it does not license the entire combined plugin under MIT**. The combined plugin is distributed under GPLv3 by choosing the later-version option of the GPL-2.0-or-later engine. Qt and other third parties retain their own licenses. Read [LICENSES.md](LICENSES.md) and [COPYING](COPYING) before redistributing binaries.

## Validation status

The final development report records 49/49 suites, 69 installer checks, 60 packaged Companion checks and two UI/device suites repeated at 150% scaling. The final DLL loads and closes in OBS portable 32.2.2 with all 23 original sources and no test exports. These are development results, not newly repeated publication tests. Release preparation verifies hashes, the language catalog and sources/license notices.

Successful installation on another PC and OBS compatibility were reported by the user and recorded as user acceptance, not a new matrix executed during publication. Real Twitch chat, a new benchmark and physical two-PC Traktor testing remain follow-up work. Not every Traktor version is certified, and no clean-machine reproducible build is claimed.

a plugin from [DjEmiMas](https://discord.gg/dhcqZPDYh)
