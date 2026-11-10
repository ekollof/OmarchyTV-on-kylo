# Glossary

Vendor and project terms used across the reports. Keep this list small and
link the owning report for the full meaning.

| term | meaning |
|---|---|
| **ABI 1 / 2 / 3** | replacement core/module interface versions. ABI 3 adds the nonblocking quiescence gate. The installed/recovery baseline is ABI 1; ABI 2 is historical; new ARM artifacts live in `build-tv-v3`. |
| **ADEC / AudioIF** | vendor audio-decoder and audio-interface blocks behind `ICE_AudioStart`/`ICE_AudioOpen`. |
| **AIN / AOUT / VAUD** | PLAT audio-in, audio-out and virtual-audio/DSP layers (`PLAT_AIN_*`, `PLAT_AOUT_*`, `PLAT_VAUD_*`). |
| **AQ / AQuality** | audio-quality volume table (101 entries from `/conf/AQ.bin`) and the `AQuality` post-param send. |
| **BLAST** | IR transmit path through `/dev/video28`, protocol 4, helper `oa-ir-blast`. |
| **CEC** | HDMI-CEC. **LA** = logical address, **PA** = physical address, **ARC** = audio return channel, **HPD** = hot-plug detect. |
| **DFB / DirectFB** | vendor framebuffer/graphics layer used by appmgr and stock UI. |
| **DIAL / EASI** | vendor DIAL app/command service and the EASI channel/service database. |
| **DRM / KMS / CRTC / plane / fb** | kernel display interfaces. The TV exposes card0/1, LVDS connectors, planes and fb0–4. |
| **EP / HP** | earphone/headphone plug detect and headphone output (`DEV_GPIO_EP_IsPlugIn`, `PLAT_VAUD_SwitchHP`). |
| **GPIO / IIC / PWM / SPI / STBC** | vendor HAL buses/controls (`Hal_Gpio_*`, `Hal_Iic_*`, `Hal_Pwm_*`, SPI, STBC IPC). |
| **ICE** | vendor app framework: `ICE_HardwareInit`, `ICE_AudioOpen/Start/Volume`, `ICE_DeviceSource`. |
| **MAU** | OSD memory-unit selection (primary vs alternate), ioctl `_IOR(0xc8,0xbd,uint32)`. |
| **MCU / watchdog** | the set's microcontroller; `/proc/nvtstbcipc` command `0xae` sets/disables the watchdog. |
| **MPFW / PQ** | picture-quality firmware block and its kernel polling task (`PQ_MPFW_TASK`). |
| **NDAL** | Novatek device abstraction layer (`libNDAL3.so`, `libNDAL_EX.so`); the `Hal_*` entry points. |
| **NIM / DTV / ATV / MXL661** | tuner front end, digital TV, analog TV, and the MXL661 tuner IC/ATV tables. |
| **OAI1** | the replacement's flat UI asset blobs (RGBA) built from theme PNGs for the legacy `ui_fb`/blit path. |
| **OSD / TCON / LVDS / VPLTIO / VPL** | on-screen display, panel timing controller, panel link, video-platform I/O and video pipeline. |
| **PTF / panel-extra** | panel power timing/PWM mode. Stock Init block `0x019354b4` byte0=1; the halfword timing fields are write-only. `PLAT_VIU_GetPanelExtraStruct` supplies the 30/580/220/30 ms delays and PTF level 100 (`*10`). |
| **PVR / EPG / EIT** | personal video recorder, electronic program guide, DVB event-information table. |
| **S/PDIF** | optical/coaxial audio output; the soundbar path. |
| **SWProfile / HWProfile** | `/conf/mb230_swprofile.bin` and `/conf/mb230_hwprofile.bin` (amp/tuner/pin/board data). |
| **UBI / ap0 / ap1 / apdat0 / fs0 / ubiblock** | NAND volumes: rootfs (fs0), `/vendor` (ap0), `/vendor2` (ap1), `/conf` (apdat0), and their read-only block mappings. |
| **Quiesce / supervisor / gate** | replacement lifecycle controls: module quiescence, `oa-supervise` watchdog/fallback, and the boot attempt gate. |
