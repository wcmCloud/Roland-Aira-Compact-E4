# Third-party notices

The project's `MIT OR GPL-3.0-only` choice covers project-authored code, documentation and original icon assets. It does not relicense dependencies or Roland material. The exact terms supplied with a dependency remain authoritative.

## JUCE and compiled applications/plug-ins

This project pins **JUCE 8.0.10**, commit `3af3ce009f6a02f6fa651008fffb5b41743a9fab`. Its framework modules are offered under **AGPLv3 or a commercial JUCE licence**. See the [upstream licence at the pinned version](https://github.com/juce-framework/JUCE/blob/8.0.10/LICENSE.md), the copied [JUCE notice](licenses/third-party/JUCE-LICENSE.md), [AGPLv3 text](licenses/third-party/JUCE-AGPL-3.0.txt), and [commercial terms](https://juce.com/legal/juce-8-licence/).

A JUCE-linked app, installer, diagnostic tool or plug-in must satisfy the applicable JUCE terms in addition to this project's terms; it is **not an MIT-only binary**. The open-source route carries AGPL requirements for the combined work, including corresponding source. The project-owned MIT alternative permits combination on that basis; choosing a project licence does not grant a commercial JUCE licence. No commercial licence entitlement is asserted by this repository. Final studio packaging includes the project/build sources and the pinned JUCE source, excluding Git history, build outputs and the separately owned Roland manual.

JUCE incorporates additional components. These copied notices are from the actual pinned source used here; some modules/codecs may be disabled or unused in a particular target:

| Component | Upstream terms / retained notice |
| --- | --- |
| VST3 SDK bundled with JUCE | [Steinberg VST3 licence or GPLv3](licenses/third-party/VST3-SDK-LICENSE.txt); retain individual-file notices. Do not substitute terms from a newer SDK for this bundled version. |
| Apple AudioUnitSDK | [Apache 2.0](licenses/third-party/AudioUnitSDK-LICENSE.txt) |
| HarfBuzz | [Old MIT and component notices](licenses/third-party/HarfBuzz-COPYING.txt) |
| SheenBidi | [Apache 2.0](licenses/third-party/SheenBidi-LICENSE.txt) |
| libpng | [PNG licence](licenses/third-party/libpng-LICENSE.txt) |
| zlib | [zlib licence](licenses/third-party/zlib-LICENSE.txt) |
| JPEG library | [Independent JPEG Group notice](licenses/third-party/JPEG-README.txt) |
| FLAC | [Retained FLAC notice](licenses/third-party/FLAC-LICENSE.txt) |
| Ogg/Vorbis | [Retained Ogg/Vorbis notice](licenses/third-party/Ogg-Vorbis-LICENSE.txt) |

The pinned JUCE source archive contains further per-file copyright/licence notices and the framework's complete dependency inventory. Preserve them when redistributing source or linked builds.

## Build and validation tools

- The optional .NET icon renderer uses **Svg.Skia 5.2.3** (MIT, [upstream](https://github.com/wieslawsoltes/Svg.Skia)) and its NuGet dependencies, including SkiaSharp. A [SkiaSharp notice](licenses/third-party/SkiaSharp-LICENSE.txt) is retained. Those rendering assemblies are not shipped in the companion/installer; generated PNG/ICNS/ICO artwork is project-authored. Restoring the optional renderer obtains packages with their own notices.
- The separately downloaded **Steinberg validator** uses SDK commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`. It is a validation tool, not shipped in the installer. Its licences remain in the ignored SDK checkout; it is distinct from the VST3 SDK embedded in JUCE.
- CMake, Apple build/signing/notarisation tools and host applications retain their own licences. None is relicensed by this project.

## Roland material and trademarks

Roland, AIRA and E-4 are trademarks of Roland Corporation. This is an independent companion, not made or endorsed by Roland. The owner's manual at `docs/E-4_eng01_W.pdf` was supplied as a development reference; it is Roland's material and is excluded from the project licences and studio installer/source archive. The icon is an original generic device/microphone drawing and contains no Roland logo. The repository does not grant rights to manufacturer material.
