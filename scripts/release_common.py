"""Shared release metadata, licence files and corresponding-source packaging."""
from pathlib import Path
import re
import shutil
import tarfile

ROOT = Path(__file__).resolve().parents[1]
JUCE_REVISION = '3af3ce009f6a02f6fa651008fffb5b41743a9fab'

def version():
    return re.search(r'project\(AiraE4 VERSION ([0-9.]+)', (ROOT/'CMakeLists.txt').read_text()).group(1)

def framework():
    for path in [ROOT/'JUCE', ROOT/'build/_deps/juce-src']:
        if (path/'CMakeLists.txt').is_file():
            return path
    raise RuntimeError('Pinned JUCE sources required: checkout JUCE 8.0.10 into JUCE/')

def copy_legal(destination):
    destination.mkdir(parents=True, exist_ok=True)
    for name in ['LICENSE.md','LICENSE-MIT','LICENSE-GPL-3.0','THIRD_PARTY_NOTICES.md']:
        shutil.copy2(ROOT/name,destination/name)
    shutil.copytree(ROOT/'licenses',destination/'licenses')

def source_archive(destination):
    destination.parent.mkdir(parents=True,exist_ok=True)
    def included(info):
        parts=Path(info.name).parts
        if any(p in ('.git','bin','obj','__pycache__','Backup','Ableton Project Info') for p in parts):return None
        if parts[-1] in ('.DS_Store','Icon\r','E-4_eng01_W.pdf') or parts[-1].endswith('.env'):return None
        return info
    with tarfile.open(destination,'w:gz') as archive:
        for name in ['Source','Tools','Tests','assets','docs','licenses','packaging','scripts','.github',
                     'CMakeLists.txt','README.md','PLAN.md','LICENSE.md','LICENSE-MIT',
                     'LICENSE-GPL-3.0','THIRD_PARTY_NOTICES.md','.gitignore']:
            if (ROOT/name).exists():archive.add(ROOT/name,arcname='E4-Companion-source/'+name,filter=included)
        archive.add(framework(),arcname='E4-Companion-source/JUCE',filter=included)
