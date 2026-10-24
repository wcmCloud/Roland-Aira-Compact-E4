#!/usr/bin/env python3
"""Build a macOS package and disk image; optionally sign, notarize and staple."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shlex
import shutil
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--build-dir', default='build-installer')
p.add_argument('--config', default='Release')
p.add_argument('--signing-config', type=Path)
p.add_argument('--notarize', action='store_true')
a = p.parse_args()
settings = dict(os.environ)
if a.signing_config:
    # Parse data only; never execute a configuration file as shell code.
    for line in a.signing_config.read_text().splitlines():
        if '=' not in line or line.lstrip().startswith('#'):
            continue
        key, value = line.split('=', 1)
        if key.strip() in ('SIGN_IDENTITY', 'INSTALLER_IDENTITY', 'NOTARY_PROFILE'):
            settings.setdefault(key.strip(), shlex.split(value)[0])
identity = settings.get('SIGN_IDENTITY', '')
installer_identity = settings.get('INSTALLER_IDENTITY', '')
profile = settings.get('NOTARY_PROFILE', '')
if a.notarize and (not identity or not profile):
    p.error('--notarize requires SIGN_IDENTITY and NOTARY_PROFILE')

def run(*args):
    subprocess.run([str(x) for x in args], check=True)

build = (ROOT / a.build_dir).resolve()
artifacts = build / f'AiraE4_artefacts/{a.config}'
source_app = artifacts / 'Standalone/E-4 Companion.app'
info = plistlib.loads((source_app / 'Contents/Info.plist').read_bytes())
version = info['CFBundleShortVersionString']
arch = subprocess.check_output(['lipo', '-archs', str(source_app / 'Contents/MacOS/E-4 Companion')], text=True).strip().replace(' ', '-')
out = ROOT / 'build-packages' / f'{version}-{arch}'
if out.exists():
    # Keep earlier release artifacts recoverable rather than overwrite them.
    import datetime
    out.rename(out.with_name(out.name + '-backup-' + datetime.datetime.now().strftime('%Y%m%d%H%M%S%f')))
out.mkdir(parents=True)
work = out / 'work'
work.mkdir()
image = work / 'image'
image.mkdir()
entries = [('VST3', 'E-4 Companion.vst3', 'Library/Audio/Plug-Ins/VST3', 'VST3'),
           ('AU', 'E-4 Companion.component', 'Library/Audio/Plug-Ins/Components', 'AU (experimental MIDI output)'),
           ('Standalone', 'E-4 Companion.app', 'Applications', 'Standalone app')]
for fmt, name, target, label in entries:
    bundle = image / name
    run('ditto', artifacts / fmt / name, bundle)
    args = ['codesign', '--force', '--sign', identity or '-']
    args += ['--timestamp', '--options', 'runtime'] if identity else ['--timestamp=none']
    run(*args, bundle)
    run('codesign', '--verify', '--deep', '--strict', bundle)

shutil.copy2(ROOT / 'packaging/macos/Install.command', image / 'Install.command')
(image / 'Install.command').chmod(0o755)
(image / 'Read Me.txt').write_text('''E-4 Companion — macOS Apple Silicon release preview

Quit audio hosts. Double-click Install.command to install VST3, AU and standalone
for your user account without administrator privileges. Existing bundles are
backed up under ~/Library/Application Support/E-4 Companion/Installer Backups.
The terminal shows installation progress. Restart/rescan your host afterwards.
You may instead copy only the formats you need to their standard plug-in folders.

AU MIDI output in Ableton Live is unsupported; use VST3 with the documented
second-track MIDI route. This companion requires the physical Roland E-4 and
produces no audio. Enable VOCODER, KEY IN and HARMONY on the hardware.

The separate .pkg installs selected formats system-wide and requests administrator
authorisation. Avoid keeping both system and user copies: user VST3 copies take
priority. The package does not delete user copies, preferences, banks or sessions.
To uninstall, quit hosts and move only the installed E-4 Companion bundles to Trash.
''')

# Component packages use explicit roots: no relocation to earlier user copies.
packages = work / 'components'
packages.mkdir()
dist = ET.Element('installer-gui-script', {'minSpecVersion': '2'})
ET.SubElement(dist, 'title').text = 'E-4 Companion'
ET.SubElement(dist, 'options', {'customize': 'always', 'require-scripts': 'false', 'hostArchitectures': arch.replace('-', ',')})
ET.SubElement(dist, 'domains', {'enable_anywhere': 'true', 'enable_currentUserHome': 'false', 'enable_localSystem': 'true'})
ET.SubElement(dist, 'allowed-os-versions')
os_versions = dist.find('allowed-os-versions')
ET.SubElement(os_versions, 'os-version', {'min': '11.0'})
choices = ET.SubElement(dist, 'choices-outline')
for fmt, name, target, label in entries:
    payload = work / ('payload-' + fmt)
    destination = payload / target / name
    destination.parent.mkdir(parents=True)
    run('ditto', image / name, destination)
    component = work / (fmt + '.plist')
    run('pkgbuild', '--analyze', '--root', payload, component)
    data = plistlib.loads(component.read_bytes())
    for item in data:
        item['BundleIsRelocatable'] = False
        item['BundleIsVersionChecked'] = True
        item['BundleHasStrictIdentifier'] = True
        item['BundleOverwriteAction'] = 'upgrade'
    component.write_bytes(plistlib.dumps(data))
    package_id = 'com.chris.aira-e4.installer.' + fmt.lower()
    run('pkgbuild', '--root', payload, '--component-plist', component,
        '--identifier', package_id, '--version', version, '--install-location', '/', packages / (fmt + '.pkg'))
    ET.SubElement(choices, 'line', {'choice': fmt})
    choice = ET.SubElement(dist, 'choice', {'id': fmt, 'title': label, 'start_selected': 'true'})
    ET.SubElement(choice, 'pkg-ref', {'id': package_id})
    ET.SubElement(dist, 'pkg-ref', {'id': package_id, 'version': version}).text = fmt + '.pkg'
distribution = work / 'Distribution.xml'
ET.ElementTree(dist).write(distribution, encoding='utf-8', xml_declaration=True)
pkg = out / f'E4-Companion-{version}-macOS-{arch}{"" if installer_identity else "-UNSIGNED"}.pkg'
args = ['productbuild', '--distribution', distribution, '--package-path', packages]
if installer_identity:
    args += ['--sign', installer_identity, '--timestamp']
run(*args, pkg)
dmg = out / f'E4-Companion-{version}-macOS-{arch}.dmg'
run('hdiutil', 'create', '-volname', 'E-4 Companion', '-srcfolder', image, '-format', 'UDZO', dmg)
if identity:
    run('codesign', '--force', '--sign', identity, '--timestamp', dmg)
if a.notarize:
    targets = [dmg] + ([pkg] if installer_identity else [])
    for artifact in targets:
        result = subprocess.run(['xcrun', 'notarytool', 'submit', str(artifact), '--keychain-profile', profile,
                                 '--wait', '--timeout', '15m', '--output-format', 'json'], capture_output=True, text=True)
        (out / (artifact.suffix[1:] + '-notary.json')).write_text(result.stdout)
        if result.returncode:
            raise RuntimeError(result.stderr)
        report = json.loads(result.stdout)
        if report.get('status') != 'Accepted':
            run('xcrun', 'notarytool', 'log', report['id'], '--keychain-profile', profile, out / 'notary-failure.json')
            raise RuntimeError('Apple did not accept the artifact; see notarization log')
        run('xcrun', 'stapler', 'staple', artifact)
        run('xcrun', 'stapler', 'validate', artifact)
        run('spctl', '--assess', '--type', 'install' if artifact == pkg else 'open',
            *([] if artifact == pkg else ['--context', 'context:primary-signature']), '--verbose=2', artifact)
(out / 'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(f.read_bytes()).hexdigest() + '  ' + f.name + '\n' for f in [pkg, dmg]))
print(f'Artifacts: {out}')
if not installer_identity:
    print('PKG is unsigned: Developer ID Installer identity required for PKG distribution. DMG notarization is independent.')
