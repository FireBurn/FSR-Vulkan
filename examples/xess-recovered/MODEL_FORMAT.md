# XESSVK1 model container

This runtime asset contains the recovered quality graph's fourteen SPIR-V
shaders and thirty-nine model buffers. The offline native bundle supplies the
payloads; the container stores no paths, process addresses, input frames or
captured frame constants. Runtime dimensions are selected through the API.

All integers are little endian. The header is 16 bytes:

| Offset | Type | Value |
| --- | --- | --- |
| 0 | 8 bytes | `XESSVK1` followed by NUL |
| 8 | uint32 | Version 1 |
| 12 | uint32 | Entry count 53 |

The next 53 records are 24 bytes each:

| Offset within record | Type | Meaning |
| --- | --- | --- |
| 0 | uint64 | Absolute payload byte offset |
| 8 | uint64 | Payload byte size |
| 16 | uint32 | IEEE CRC32 of the payload |
| 20 | uint32 | Reserved, zero |

Each payload starts at the next 16-byte boundary after the previous payload
(the first follows the table). Sizes are nonzero and at most 4 MiB. The file
ends immediately after the final payload and is at most 64 MiB. These rules
reject overlapping records, arbitrary gaps, out-of-bounds offsets and trailing
data. Padding is emitted as zero but is not part of payload CRCs.

Entry 0 is prepare SPIR-V and entry 1 is resolve SPIR-V. Each convolution
`i=0..11` occupies entries `2+4*i` through `5+4*i`: shader, weights, scale and
bias. Entries 50..52 are the resolve weight/scale/bias CBVs, including explicit
padding, with sizes 512/256/256 bytes.

`ffxVkXeSSRecoveredModelFromBlob` returns spans borrowing these bytes. It checks
container structure and CRCs without executing GPU work. `Create` checks shader
headers and the fixed model buffer sizes, copies the payloads and creates the
Vulkan graph. Container validation alone does not establish model execution.
