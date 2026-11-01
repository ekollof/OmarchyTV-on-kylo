# Recovery and access verification — 2026-10-31

The owner successfully recovered the TV with the exact older `apply_hook.scr.txt`
installed as `kylo_usb_update.scr` on the 8 GB FAT32 TVHACK stick. The 584-byte
script has MD5 `de1591298d633cf6422a1ade4e8835a9`; the restored UBIFS image has
MD5 `994da7ff76a0b2fbafb8f6fa0c682627`. Both are in `artifacts/recovery-stability/`.
The earlier unsuccessful 549-byte version differs only in comments/blank lines;
the reason those attempts did not complete is not established. Do not attribute
the success to a proven parser defect.

## Verified after recovery

- Fresh key-authenticated SSH connection to `192.168.178.87:22`: uid 0.
- Fresh telnet connection to port 2323: shell `id` returned uid 0.
- New kernel boot ID `ebb0aaaa-72a2-4410-9f8d-95b9249f8a90`; uptime advanced
  through 62 seconds during verification.
- Ethernet up, MAC `70:54:b4:fe:70:c6`, IPv4 `192.168.178.87`, LAN route and
  default route through `192.168.178.1` present. No gateway probes were sent.
- `/etc/hosts` byte-identical to the persistent blocklist; Samba OUTPUT REJECT
  for `216.183.117.0/24` present.
- SQLite values: internet=1; advertising/personalised/marketing/third-party/ACR=0;
  DNT=1, logging=0, wake-on=1.
- Root home and .ssh permissions 700; working hook and keeper match the image.
- One keepalive loop, PID 2234, with SSH listener PID 1811 and telnet/DIAL up.
- `/conf/oa_safe` and `/conf/coredumpnoreset` present, native boot marker absent;
  no openaurora process running. Stock aurora present.

Hook MD5: `0b1cd32a9de2b0251f6252e6603db8b7`.
Keeper MD5: `4ebd0f9d7ee9f470afde8b0f2807522b`.
The local loose hook was restored to match this working version; no new hook was
uploaded after recovery.

## Persistent maintenance

The hook launches `/conf/remotelf/keepalive.sh` independently of UI callbacks.
It checks actual SSH/telnet listeners, repairs root-home/key permissions, and
checks the hosts block and Samba rule. Its idle sleep is five seconds; bounded
SQLite/DHCP work can extend the interval. Privacy/internet/WoL settings are checked
every twelve passes, with updates restricted to mismatching values. Ethernet is
brought up if administratively down; bounded DHCP requests are retried when
address/default route is missing and no other udhcpc is running.

Before the deployment mistake, stopping each listener separately resulted in
automatic recovery within seven seconds and successful fresh connections. A
controlled reboot also passed the access/privacy checks. The final upload then
likely truncated the hook due to a backgrounded shell AND-list, requiring the
physical recovery above. That failure and the command-construction lesson are
recorded in MEMORY.md. It was not a successful uninterrupted stability session.

The current UBIFS image includes maintenance, privacy settings, accepted keys and
the stock-UI safety flag. The raw mtd13 image and pre-stability backup images remain
older; do not use their presence as evidence that they contain this setup.

Standby still suspends network services. Wake-on-LAN has historical validation;
it is not an always-on SSH connection or proof of deep-standby reachability.

## Owner constraint

The TV is now in use. SSH/read-only diagnostics are permitted. No deployments,
service restarts, network changes, UI keys, power-state changes or reboots until
the owner authorizes them. Post-recovery verification was read-only. No further
on-set changes are required for the current task.
