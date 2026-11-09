# PQ_MPFW_TASK and the load-average artifact — 2026-11-09

Status: **read-only on-TV and offline driver analysis. No change made.**

## Question

`PQ_MPFW_TASK` and about a dozen other kernel threads sit in `D` state and the
set reports a load average of ~15 even while idle. Is something spinning, and
does the load number mean the CPU is saturated?

## Identification

`PQ_MPFW_TASK` is a vendor kernel thread from
`vendor2/modules/ntkdriver_nondovi.ko` (MD5
`fded7005651e37b8a1b5e8e89f73f4ff`). The task name string is in the driver's
`.rodata`; the thread function is `PLAT_PQ_Mpfw_CtrlTask` at `0x253d84`
(section-relative `.text` offset in the extracted module).

The loop (objdump of the extracted module):

```asm
253d9c  bl PQ_OS_GetClockInMS
253de0  bl PQ_OS_GetClockDiff
253de4  cmp r0, #19
253dec  rsb r0, r0, #20
253df0  bl PQ_OS_TaskDelayInMS
...
3788d8 <PQ_OS_TaskDelayInMS>:
3788ec  bl msleep
```

So it is a **50 Hz (20 ms) polling task** for the PQ/MPFW firmware interface:
`PLAT_PQ_Mpfw_GetPollingCnt`, `PLAT_PQ_Mpfw_DbgMsgFilter`, `PLAT_PQ_Mpfw_SetPQ`
and the MPFW IRQ/shell hooks are its neighbours. It is not a tight spin loop.

## Why the load average is ~15

On Linux 4.9 `msleep()` sleeps in `TASK_UNINTERRUPTIBLE`. The load average
counts runnable tasks **and** uninterruptible (D) sleepers. The driver leaves
about 15 polling kthreads parked in D:

```text
PQ_MPFW_TASK, PQ_SHELL_DBG_TA, _VPL_SRC_KADC, _NerveMTask_,
VPL_SCLR_CTRL_M, TCON_ARM_TASK, SRC_MonitorTask, SRC_ModeTask,
PLAT_WDT, MHL_MID_EvtThre, KER_CI_Timer_Ta, DRV_VD, DRV_VBI,
DRV_TVADC, DRV_DBG_UART
```

Observed on the stock set:

```text
/proc/loadavg: 15.07 15.12 15.09 4/501
states:        15 x D
wchan:         msleep (PQ_MPFW_TASK, PQ_SHELL_DBG_TA, PLAT_WDT, DRV_VD,
                TCON_ARM_TASK); _NerveMTask_ reports kthread
```

Each D task contributes 1.0, so an otherwise idle set reads ~15. A 3 s
`/proc/stat` sample:

```text
total=1321 idle=1181 iowait=0 -> busy=10.6% idle=89.4% iowait=0.0%
PQ_MPFW_TASK over 5 s: utime +0, stime +23 jiffies (~4.6% of one core at HZ=100)
```

The task's CPU use is consistent with a 50 Hz poller doing a little work per
wake; the CPU is ~89% idle. `/proc/PID/stack` on this kernel returns an invalid
frame (`0xffffffff`), so the call stack could not be unwound; `wchan=msleep`
and the disassembly are the evidence.

## Conclusion

- `PQ_MPFW_TASK` is the vendor PQ MPFW control/polling kthread, not an
  OpenAurora task and not a hot spin.
- The load average is a **D-state accounting artifact** of the vendor's
  `msleep` pollers. It is not a CPU-capacity signal on this set.
- This is stock behavior; killing the thread or patching the signed driver
  would interfere with PQ/firmware handling and is out of scope.
- For acceptance, use CPU idle, per-process CPU time or the app's own timings,
  not the load average. The stock baseline is ~15.

## Evidence

- On-TV: `/proc/loadavg`, per-task state/wchan/stack, `/proc/stat` and
  `/proc/<pid>/stat` samples (read-only).
- Offline: `strings`/`nm`/`objdump -dr` on
  `artifacts/workbench/rootfs/vendor2/modules/ntkdriver_nondovi.ko`
  (`PLAT_PQ_Mpfw_CtrlTask` 0x253d84, `PQ_OS_TaskDelayInMS` 0x3788d8).
- Related: [VENDOR-INVENTORY](VENDOR-INVENTORY-2026-11-07.md) covers the module
  set; [FINDINGS](../../FINDINGS.md) owns corrections.
