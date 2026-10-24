#!/usr/bin/env python3
"""Generate short, deterministic MIDI files for manual host/hardware acceptance."""
from pathlib import Path
import struct

out = Path(__file__).resolve().parents[1] / 'docs/studio/fixtures'
out.mkdir(parents=True, exist_ok=True)

def vlq(value):
    result = [value & 127]
    while value > 127:
        value >>= 7
        result.insert(0, (value & 127) | 128)
    return bytes(result)

def write(name, events, end):
    data = b'\x00\xff\x51\x03\x07\xa1\x20'  # 120 BPM
    previous = 0
    for tick, message in sorted(events, key=lambda event: event[0]):
        data += vlq(tick - previous) + bytes(message)
        previous = tick
    data += vlq(end - previous) + b'\xff\x2f\x00'
    (out / name).write_bytes(b'MThd' + struct.pack('>IHHH', 6, 0, 1, 480) + b'MTrk' + struct.pack('>I', len(data)) + data)

write('four-voices-and-fifth.mid',
      [(0, [0x90, n, 64]) for n in [60,64,67,71]] +
      [(480, [0x90,74,64]), (960, [0x80,74,0])] +
      [(1440, [0x80,n,0]) for n in [60,64,67,71]], 1920)
write('pitch-bend.mid', [(0,[0xe0,0,64]), (0,[0x90,60,64]),
      (480,[0xe0,0,0]), (960,[0xe0,127,127]), (1440,[0xe0,0,64]),
      (1920,[0x80,60,0])], 2400)
write('controller-mapping-input.mid', [(0,[0x90,48,64]), (480,[0x80,48,0]),
      (960,[0xb0,20,127]), (1440,[0x90,60,64]), (1920,[0x80,60,0]),
      (2160,[0xb0,20,64])], 2400)
print('Generated three channel-1 fixtures at 120 BPM; no ports opened.')
# Version-1 .e4map: note48/channel1 -> Pad1; CC20/channel1 -> transpose.
# Validated against ControllerPreset::decode during release preflight.
import base64
rows = [[1,1,48,0,1,64,2], [2,1,20,4,1,64,2]] + [[0,0,0,0,1,64,2] for _ in range(6)]
(out / 'studio-controller.e4map').write_text(base64.b64encode(b'E4MP\x01' + bytes(v for row in rows for v in row)).decode())
