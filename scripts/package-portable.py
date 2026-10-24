#!/usr/bin/env python3
"""Package native CI builds; macOS archives are development builds, not notarized."""
import argparse
import hashlib
from pathlib import Path
import shutil
import tarfile
from release_common import ROOT, version, copy_legal, source_archive

p=argparse.ArgumentParser()
p.add_argument('--build-dir', type=Path, required=True)
p.add_argument('--platform', choices=['Windows','Linux','macOS'], required=True)
p.add_argument('--arch', choices=['x64','arm64'], required=True)
a=p.parse_args()
name=f'E4-Companion-{version()}-{a.platform}-{a.arch}'
out=ROOT/'build-release-assets'; out.mkdir(exist_ok=True)
stage=ROOT/'build-release-stage'/name
if stage.exists(): shutil.rmtree(stage)
stage.mkdir(parents=True)
build=a.build_dir.resolve()
products=build/'AiraE4_artefacts/Release'
shutil.copytree(products/'VST3',stage/'VST3')
shutil.copytree(products/'Standalone',stage/'Standalone')
if a.platform=='macOS': shutil.copytree(products/'AU',stage/'AU')
probe=build/'e4_midi_probe_artefacts/Release'/('e4_midi_probe.exe' if a.platform=='Windows' else 'e4_midi_probe')
shutil.copy2(probe,stage/probe.name)
copy_legal(stage/'Licences')
shutil.copytree(ROOT/'docs/studio',stage/'Studio-test-kit')
source_archive(stage/'Sources'/f'E4-Companion-{version()}-source.tar.gz')
(stage/'INSTALL.txt').write_text((ROOT/'docs/RELEASE-NOTES.md').read_text(encoding='utf-8'), encoding='utf-8')
if a.platform=='Linux':
    shutil.copy2(ROOT/'assets/icon/png/app-icon-256.png',stage/'e4-companion.png')
    shutil.copy2(ROOT/'packaging/linux/install.sh',stage/'install.sh')
    (stage/'install.sh').chmod(0o755)
    with tarfile.open(out/(name+'.tar.gz'),'w:gz') as archive: archive.add(stage,arcname=name)
else:
    if a.platform=='Windows': shutil.copy2(ROOT/'packaging/windows/install.ps1',stage/'install.ps1')
    shutil.make_archive(str(out/name),'zip',stage.parent,stage.name)
for file in out.iterdir():
    if file.is_file() and file.name.startswith(name) and not file.name.endswith('.sha256'):
        (out/(file.name+'.sha256')).write_text(hashlib.sha256(file.read_bytes()).hexdigest()+'  '+file.name+'\n')
print(out)
