#!/usr/bin/env python3
"""Install local development bundles, backing up existing copies and rolling back failures."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', default='build-check')
parser.add_argument('--config', default='Debug')
parser.add_argument('--au', action='store_true', help='Also install experimental AU')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
build = (root / args.build_dir).resolve()
formats = [('VST3', 'vst3', 'VST3')]
if args.au:
    formats.append(('AU', 'component', 'Components'))
stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')
backup = build / 'plugin-install-backups' / stamp
backup.mkdir(parents=True)
manifest = []
changed = []
try:
    for fmt, extension, folder in formats:
        source = build / f'AiraE4_artefacts/{args.config}/{fmt}/E-4 Companion.{extension}'
        if not source.is_dir():
            raise RuntimeError(f'Build first: missing {source}')
        # Development-only ad-hoc signature; does not provide notarisation.
        subprocess.run(['codesign', '--force', '--sign', '-', '--timestamp=none', str(source)], check=True)
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(source)], check=True)
    for fmt, extension, folder in formats:
        source = build / f'AiraE4_artefacts/{args.config}/{fmt}/E-4 Companion.{extension}'
        destination = Path.home() / f'Library/Audio/Plug-Ins/{folder}' / source.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        previous = backup / source.name
        if destination.exists():
            shutil.move(str(destination), str(previous))
        changed.append((destination, previous))
        subprocess.run(['ditto', str(source), str(destination)], check=True)
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(destination)], check=True)
        hashes = {}
        for file in source.rglob('*'):
            if file.is_file():
                relative = file.relative_to(source)
                digest = hashlib.sha256(file.read_bytes()).hexdigest()
                if digest != hashlib.sha256((destination / relative).read_bytes()).hexdigest():
                    raise RuntimeError(f'Installed file differs: {relative}')
                hashes[str(relative)] = digest
        manifest.append({'format': fmt, 'source': str(source), 'installed': str(destination),
                         'previous': str(previous) if previous.exists() else None, 'sha256': hashes})
except Exception:
    for destination, previous in reversed(changed):
        if destination.exists():
            shutil.rmtree(destination)
        if previous.exists():
            shutil.move(str(previous), str(destination))
    raise
(backup / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(f'Installed and verified. Backup/manifest: {backup}')
