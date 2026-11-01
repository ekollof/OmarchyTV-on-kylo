# OSD memory selection — firmware trace and read-only TV confirmation

> **Status: Current GET-query contract plus a dated real-TV observation. It does not explain previous invisible output.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Driver source of truth: ntkdriver_nondovi.ko, MD5
`fded7005651e37b8a1b5e8e89f73f4ff`. Continued the kernel callback trace.

The framebuffer ioctl wrapper handles these low-byte commands:

- `0xbc`: set preferred MAU using the argument value itself, not a pointer.
- `0xbd`: copy one uint32 from `gu32_current_mau[plane]` to the caller. This
  branch reads cached state and invokes no hardware callback or setter.
- `0xbe`: copy 12 bytes, validate MAU < 2 and yoffset within virtual height,
  require a nonzero separate-buffer parameter, then schedule a later switch.

The pan handler consumes that pending switch only at the specified yoffset.
It selects the primary fb physical base when preference is zero, or the alternate
`gast_osd_dmabuf` base otherwise, and records the committed choice in current_mau.
Thus ordinary fb mmap and current scanout can refer to different physical regions.
This is a firmware-supported condition, not proof of the earlier invisible-output cause.

The dma-buf exporter can combine two physical regions into one exported mapping;
its mmap walks segment sizes and maps the selected physical ranges. Exporting a
dma-buf allocates state, so the diagnostic below does not use that API.
Raw decompilation: `artifacts/analysis/osd-2026-10-31/oa-osd-mau-export.txt`;
selection branches are in the previously saved ioctl/pan logs.

## Diagnostic and actual TV observation

Extended `tools/mcu/fbregion.c` to open O_RDONLY and GET fixed screen info and
current MAU in addition to variable screen info and region. Current-MAU request
is `_IOR(0xc8,0xbd,uint32_t)`; the driver masks the ioctl command to its low byte
and copies exactly four bytes. No SET/PAN/export requests are made.

Wrapped-device tests accept only those four GET operations and assert O_RDONLY.
They pass on host and ARM/qemu with -Werror -pedantic. The helper builds as
`make BUILD=... .../fbregion`; it is not installed by the normal build/install.

After the owner explicitly allowed querying, staged the ARM helper under a
temporary /tmp directory and separately verified MD5
`585fc149cfa834673510ebc40d93fe7c` and size 16,408 bytes. One fb1 query returned:

```text
1920x1080, virtual 1920x2160, 32bpp, offset 0,1080
primary physical base 0x35bbb000, length 16588800, stride 7680
current MAU 0 (primary selected)
region: 0,0,1920,1080,0,0,3840,2160
```

This snapshot shows no alternate-memory mismatch in the running vendor UI.
It does not establish the state during a previous takeover. Do not change memory
selection based on the historical symptom alone. Preserve the separate source
and destination region dimensions rather than treating the last two words as
arbitrary buffer bounds.

Removed the helper and temporary directory. Vendor PIDs remained 1899/1737;
boot ID remained ebb0aaaa-72a2-4410-9f8d-95b9249f8a90, uptime 3321.11 seconds.
No drawing, mode changes, service restart, network changes or persistent install.
Complete query output: `artifacts/analysis/osd-2026-10-31/oa-mau-live.txt`.

Still open: separate-buffer value 2's full allocation policy, zero-buffer paths,
and reconciliation of the userspace GEM surface sizes with kernel plane memory.
