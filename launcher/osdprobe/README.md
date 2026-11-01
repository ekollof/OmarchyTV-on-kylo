# Historical device probes

These are research tools, not runtime backends or an approved probe sequence.
The TV is in use. See ../../AGENTS.md and ../../FINDINGS.md before any execution.

- drmprobe without --paint enumerates DRM state. Its paint mode uses the old
  generic dumb-buffer hypothesis; target firmware actually allocates only 4 KiB
  there. A large mmap failure does not rule out the private GEM/OSD path. Do not
  repeat paint/master experiments as the next display step.
- irkey consumes stock UI UART bytes and configures the tty. It is not a harmless
  view-only query; do not run it alongside the in-use vendor UI. Frame/code-space
  and repeat/release interpretations remain incomplete.

Bundled compiled helpers may predate current fixes/flags. Check the matching
source, ABI/toolchain and file identity before any future authorized staging.
The O_RDONLY GET-only framebuffer diagnostic maintained/tested for development
is ../../openaurora/tools/mcu/fbregion.c, built into build-tv-v2 explicitly.
