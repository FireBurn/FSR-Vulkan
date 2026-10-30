#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Compile and package d4r / DLSS SPIR-V compute kernels into C headers."""
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess

root = Path(__file__).resolve().parents[1]
shader_dir = root / 'shaders/dlss'
include_dir = root / 'include'

glslang = shutil.which('glslangValidator')
validator = shutil.which('spirv-val')
if not glslang or not validator:
    raise SystemExit('glslangValidator and spirv-val are required')

shaders = [
    ('d4r_swin_reconstruct', shader_dir / 'd4r_swin_reconstruct.comp', include_dir / 'd4r_swin_reconstruct_spv.h'),
    ('d4r_input_preprocess', shader_dir / 'd4r_input_preprocess.comp', include_dir / 'd4r_input_preprocess_spv.h'),
    ('d4r_swin_encoder', shader_dir / 'd4r_swin_encoder.comp', include_dir / 'd4r_swin_encoder_spv.h'),
    ('d4r_swin_decoder', shader_dir / 'd4r_swin_decoder.comp', include_dir / 'd4r_swin_decoder_spv.h'),
    ('d4r_dfn_reconstruct', shader_dir / 'd4r_dfn_reconstruct.comp', include_dir / 'd4r_dfn_reconstruct_spv.h'),
]

for name, comp_path, h_path in shaders:
    spv_path = comp_path.with_suffix('.spv')
    print(f'Compiling {comp_path.name} -> {spv_path.name}...')
    subprocess.run([glslang, '-V', '--target-env', 'vulkan1.3', '-Os', '-o', str(spv_path), str(comp_path)], check=True)
    subprocess.run([validator, '--target-env', 'vulkan1.3', str(spv_path)], check=True)

    data = spv_path.read_bytes()
    word_count = len(data) // 4
    words = struct.unpack(f'<{word_count}I', data)
    sha = hashlib.sha256(data).hexdigest()

    guard = f'{name.upper()}_SPV_H'
    lines = [
        f'/* Auto-generated from {comp_path.relative_to(root)}; do not edit. */',
        f'/* SHA256: {sha} */',
        f'#ifndef {guard}',
        f'#define {guard}',
        '',
        '#include <stdint.h>',
        '#include <stddef.h>',
        '',
        f'static const uint32_t g_{name}_spv[] = {{'
    ]
    for i in range(0, len(words), 8):
        chunk = words[i:i + 8]
        lines.append('    ' + ', '.join(f'0x{w:08x}u' for w in chunk) + ',')
    lines.append('};')
    lines.append('')
    lines.append(f'static const size_t g_{name}_spv_word_count = sizeof(g_{name}_spv) / sizeof(uint32_t);')
    lines.append(f'static const size_t g_{name}_spv_byte_size = sizeof(g_{name}_spv);')
    lines.append(f'static const size_t g_{name}_spv_size = sizeof(g_{name}_spv);')
    lines.append('')
    lines.append(f'#endif /* {guard} */')
    lines.append('')

    h_path.write_text('\n'.join(lines), encoding='utf-8')
    print(f'  Wrote {h_path.name} ({len(data)} bytes, {word_count} words)')
