# E-4 Companion app icon

Original editable SVG in the same metallic-device style as the user's T-8 app icon. The microphone, four rings and four performance pads distinguish the vocoder companion; colours follow the E-4 Companion interface. No manufacturer logo or wordmark is included.

`./scripts/render-icon.sh` uses the project-local .NET 10/Svg.Skia renderer to generate transparent PNGs (16–1024), macOS ICNS and Windows ICO. Generated assets are committed so ordinary JUCE builds do not need .NET. JUCE embeds the PNG source into platform bundle icons for the standalone, plug-in shells and macOS graphical installer.
