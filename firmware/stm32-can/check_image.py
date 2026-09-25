"""Check built artifact consistency and vector bindings, without opening a probe.

Uses pyelftools already installed in the project's pyOCD environment.
"""
import hashlib
import json
import struct
from pathlib import Path
from elftools.elf.elffile import ELFFile

ROOT = Path(__file__).resolve().parent
data = (ROOT / 'build/canbench.bin').read_bytes()
manifest = json.loads((ROOT / 'build/artifact-sha256.json').read_text(encoding='utf-8-sig'))
for item in manifest:
    blob = (ROOT / 'build' / item['File']).read_bytes()
    assert len(blob) == item['Bytes']
    assert hashlib.sha256(blob).hexdigest() == item['SHA256']

addresses = {}
upper = 0
eof = False
for line in (ROOT / 'build/canbench.hex').read_text().splitlines():
    assert line.startswith(':') and not eof
    record = bytes.fromhex(line[1:])
    assert sum(record) % 256 == 0 and len(record) == record[0] + 5
    size, offset, kind = record[0], int.from_bytes(record[1:3], 'big'), record[3]
    payload = record[4:4 + size]
    if kind == 0:
        for i, byte in enumerate(payload):
            address = upper + offset + i
            assert address not in addresses
            addresses[address] = byte
    elif kind == 4:
        upper = int.from_bytes(payload, 'big') << 16
    elif kind == 1:
        eof = True
    else:
        assert kind in (3, 5), kind
assert eof and len(addresses) == len(data)
assert min(addresses) == 0x08000000 and max(addresses) == 0x08000000 + len(data) - 1
assert bytes(addresses[a] for a in sorted(addresses)) == data
assert 0 < len(data) <= 512 * 1024

with (ROOT / 'build/canbench.axf').open('rb') as stream:
    elf = ELFFile(stream)
    assert elf['e_machine'] == 'EM_ARM'
    symbols = {s.name: s['st_value'] for s in elf.get_section_by_name('.symtab').iter_symbols()}

sp = struct.unpack_from('<I', data, 0)[0]
assert 0x20000000 < sp <= 0x20010000 and sp % 8 == 0
vectors = {'Reset_Handler': 1, 'HardFault_Handler': 3, 'SysTick_Handler': 15,
           'USB_HP_CAN1_TX_IRQHandler': 16 + 19,
           'USB_LP_CAN1_RX0_IRQHandler': 16 + 20,
           'CAN1_SCE_IRQHandler': 16 + 22}
bindings = {}
for name, index in vectors.items():
    value = struct.unpack_from('<I', data, index * 4)[0]
    assert value == (symbols[name] | 1), (name, value, symbols[name])
    assert 0x08000000 <= (value & ~1) < 0x08000000 + len(data)
    bindings[name] = hex(value)
assert len(set(bindings.values())) == len(bindings)

upstream = json.loads((ROOT / 'UPSTREAM.json').read_text(encoding='utf-8'))
for item in upstream['files']:
    blob = (ROOT / 'vendor' / item['path']).read_bytes()
    assert hashlib.sha256(blob).hexdigest() == item['sha256']

result = {'success': True, 'bin_bytes': len(data), 'initial_sp': hex(sp),
          'sha256': hashlib.sha256(data).hexdigest(), 'vectors': bindings,
          'unchanged_vendor_files': len(upstream['files']), 'hardware_verified': False}
(ROOT / 'build/image-check.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
