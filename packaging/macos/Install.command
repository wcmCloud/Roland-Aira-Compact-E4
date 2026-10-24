#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
backup="$HOME/Library/Application Support/E-4 Companion/Installer Backups/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$backup"
sources=("E-4 Companion.vst3" "E-4 Companion.component" "E-4 Companion.app")
folders=("$HOME/Library/Audio/Plug-Ins/VST3" "$HOME/Library/Audio/Plug-Ins/Components" "$HOME/Applications")
changed=0
rollback() {
  local i
  for ((i=changed-1; i>=0; i--)); do
    if [[ -e "${folders[i]}/${sources[i]}" ]]; then
      mv "${folders[i]}/${sources[i]}" "$backup/failed-$i"
    fi
    if [[ -e "$backup/${sources[i]}" ]]; then
      mv "$backup/${sources[i]}" "${folders[i]}/${sources[i]}"
    fi
  done
  echo "Installation failed; previous bundles restored. Backup: $backup"
}
trap 'rollback; exit 1' ERR
for ((i=0; i<3; i++)); do
  codesign --verify --deep --strict "${sources[i]}"
done
for ((i=0; i<3; i++)); do
  mkdir -p "${folders[i]}"
  if [[ -e "${folders[i]}/${sources[i]}" ]]; then
    mv "${folders[i]}/${sources[i]}" "$backup/${sources[i]}"
  fi
  changed=$((i+1))
  ditto "${sources[i]}" "${folders[i]}/${sources[i]}"
  codesign --verify --deep --strict "${folders[i]}/${sources[i]}"
done
trap - ERR
echo "Installed VST3, AU and standalone. Restart your audio host."
echo "Previous bundles, if present: $backup"
