#!/bin/sh
# tvhack local boot hook - runs from /conf/remotelf/aurora.elf (apdat0, no network needed)
# The attempt log lives on /buffer (ubi14), not /conf: the USB recovery only
# rewrites apdat0, so this log and the app log survive a recovery.
L=/buffer/tvhack-boot.log
S=/conf/remotelf

# mount_buffer.sh runs in parallel with appmgr; give /buffer a bounded moment
# so the log and the bundle checks below see the mounted filesystem.
i=0
while [ ! -e /tmp/mount_buffer_ready ] && [ $i -lt 20 ]; do
  sleep 1
  i=$((i+1))
done

# keep the log bounded (rotate once at boot)
if [ -f "$L" ] && [ "$(wc -c < "$L" 2>/dev/null)" -gt 200000 ]; then
  mv -f "$L" "$L.old" 2>/dev/null
fi

{
  echo "== tvhack boot $(date)"
  echo "id: $(id)"
  echo "uname: $(uname -a)"
} >>"$L" 2>&1

# Native takeover is opt-in. Validate the recovery pieces before suppressing the
# browser opener; /conf/oa_safe always selects vendor aurora.
oa_state()
{
  echo "oa: $1 boot=$([ -f /conf/oa_boot ] && echo 1 || echo 0) safe=$([ -f /conf/oa_safe ] && echo 1 || echo 0) coredump=$([ -f /conf/coredumpnoreset ] && echo 1 || echo 0) conf=$([ -r /conf/openaurora.conf ] && echo 1 || echo 0) bin=$([ -x /buffer/openaurora/openaurora ] && echo 1 || echo 0) sup=$([ -x /buffer/openaurora/oa-supervise ] && echo 1 || echo 0) wdt=$([ -x /buffer/wdtctl ] && echo 1 || echo 0) marker=$([ -e /tmp/mount_buffer_ready ] && echo 1 || echo 0)"
}
{ oa_state state; } >>"$L" 2>&1

OA_NATIVE=0
if [ -f /conf/oa_boot ] && [ ! -f /conf/oa_safe ] &&
   [ -f /conf/coredumpnoreset ] && [ -r /conf/openaurora.conf ] &&
   [ -x /buffer/openaurora/openaurora ] && [ -x /buffer/openaurora/oa-supervise ] &&
   [ -x /buffer/wdtctl ]; then
  OA_NATIVE=1
fi

# Record the decision itself. Diagnostic only.
{ echo "oa: decision native=$OA_NATIVE"; } >>"$L" 2>&1

{
  # run_aurora.sh forced a dev MAC; put the real one back and renew the lease
  ifconfig eth0 down 2>/dev/null
  ifconfig eth0 hw ether 70:54:B4:FE:70:C6 2>/dev/null
  ifconfig eth0 up 2>/dev/null
  udhcpc -i eth0 -b -q -n -t 5 -T 3 -A 3 2>/dev/null &

  mkdir -p /dev/pts
  mount -t devpts devpts /dev/pts 2>/dev/null
  echo "devpts: $?"

  # writable /etc/passwd so root's home is somewhere we own
  umask 077; mkdir -p /tmp/tvh_etc /tmp/tvh_root/.ssh; chmod 700 /tmp/tvh_root
  cp -a /etc/passwd /tmp/tvh_etc/passwd 2>/dev/null
  sed -i 's|^root:x:0:0:[^:]*:/bin:|root:x:0:0:Linux User,,,:/tmp/tvh_root:|' /tmp/tvh_etc/passwd
  mount --bind /tmp/tvh_etc/passwd /etc/passwd 2>/dev/null
  echo "passwd bind: $?"

  # blackhole telemetry / portal / search / ACR / assistive endpoints
  if [ -f "$S/hosts" ]; then
    mount --bind "$S/hosts" /etc/hosts 2>/dev/null
    echo "hosts bind: $?"
  else
    echo "hosts file missing, bind skipped"
  fi

  cp -f $S/authorized_keys /tmp/tvh_root/.ssh/authorized_keys 2>/dev/null
  chmod 700 /tmp/tvh_root/.ssh 2>/dev/null
  chmod 600 /tmp/tvh_root/.ssh/authorized_keys 2>/dev/null

  $S/dropbear -p 22 -P /tmp/dropbear.pid \
      -r $S/hostkey_rsa -r $S/hostkey_ed25519 >>$L 2>&1
  echo "dropbear rc: $?"

  telnetd -l /bin/sh -p 2323 >>$L 2>&1
  echo "telnetd rc: $?"

  # DIAL: nvt.app.debug.ini disables dialserver, so run it standalone as root.
  # Gives back SmartCenter/Dial command surface (keys, browserseturl, ...).
  if [ ! -f /conf/no_dial ]; then
    ( cd /vendor && exec ./com.nvt.dial/com.nvt.dialserver ) >>$L 2>&1 &
    sleep 1
    echo "dialserver: $(pidof com.nvt.dialserver 2>/dev/null || echo not-running)"
  fi

  # --- launcher (P2): opt-in web launcher served from /buffer -----------------
  # /conf/launcher_enable switches it on. It opens ONCE per boot (the page itself
  # records success via /cgi-bin/boot-ok -> /conf/launcher_ok). After 3 failed
  # boots the flag is removed, so a broken page can never lock the TV out of its
  # own UI. /conf/launcher_fail holds the consecutive-failure count.
  if [ "$OA_NATIVE" = 0 ] && [ -f /conf/launcher_enable ] && [ -d /buffer/launcher ]; then
    pidof httpd >/dev/null 2>&1 || ( httpd -p 8181 -h /buffer/launcher -f >>$L 2>&1 & )
    sleep 1
    echo "launcher: httpd=$(pidof httpd 2>/dev/null || echo not-running) fail_streak=$(cat /conf/launcher_fail 2>/dev/null || echo 0)"
    rm -f /conf/launcher_ok

    # opener: waits for DIAL, asks the app to load the page, records the outcome
    (
      i=0
      while [ $i -lt 15 ]; do
        i=$((i+1))
        sleep 4
        wget -q -O /dev/null -T 2 http://127.0.0.1:56790/dd.xml 2>/dev/null || continue
        wget -q -O /dev/null -T 3 \
          --header="Content-Type: text/plain; charset=ISO-8859-1" \
          --header="application_name: tv smart centre" \
          --post-data="<?xml version='1.0' ?><browserseturl><load url='http://127.0.0.1:8181/omarchy/?boot=$i' page='RC'/></browserseturl>" \
          http://127.0.0.1:56789/apps/SmartCenter 2>/dev/null
        echo "launcher: open attempt $i"
        sleep 7
        if [ -f /conf/launcher_ok ]; then
          echo "launcher: OPENED on attempt $i ($(cat /conf/launcher_ok))"
          echo 0 > /conf/launcher_fail
          break
        fi
      done
      if [ ! -f /conf/launcher_ok ]; then
        n=$(cat /conf/launcher_fail 2>/dev/null || echo 0)
        n=$((n+1))
        echo "$n" > /conf/launcher_fail
        echo "launcher: FAILED this boot (streak $n/3)"
        if [ "$n" -ge 3 ]; then
          rm -f /conf/launcher_enable
          echo "launcher: DISABLED after $n failures - re-enable with: touch /conf/launcher_enable"
        fi
      fi
    ) >>$L 2>&1 &

    # resume watcher: a page does not survive suspend and the set wakes on its source, so after a
    # "PM: suspend exit" bring the portal up (MENU) and reopen the launcher, retrying until the page
    # actually heartbeats (/conf/launcher_alive is refreshed every 5 s by the page).
    (
      dmesg -c >/dev/null 2>&1
      while [ -f /conf/launcher_enable ]; do
        if dmesg 2>/dev/null | grep -qa "PM: suspend exit"; then
          dmesg -c >/dev/null 2>&1
          mark=$(date +%s)
          echo "resume: detected at $(date)"
          n=0
          while [ $n -lt 10 ]; do
            n=$((n+1))
            wget -q -O /dev/null -T 2 http://127.0.0.1:56790/dd.xml 2>/dev/null || { sleep 3; continue; }
            wget -q -O /dev/null -T 3 \
              --header="Content-Type: text/plain; charset=ISO-8859-1" \
              --header="application_name: tv smart centre" \
              --post-data="<?xml version='1.0' ?><remote><key code='1048'/></remote>" \
              http://127.0.0.1:56789/apps/SmartCenter 2>/dev/null
            sleep 3
            wget -q -O /dev/null -T 3 \
              --header="Content-Type: text/plain; charset=ISO-8859-1" \
              --header="application_name: tv smart centre" \
              --post-data="<?xml version='1.0' ?><browserseturl><load url='http://127.0.0.1:8181/omarchy/' page='RC'/></browserseturl>" \
              http://127.0.0.1:56789/apps/SmartCenter 2>/dev/null
            sleep 6
            a=$(cat /conf/launcher_alive 2>/dev/null || echo 0)
            if [ "${a:-0}" -gt "$mark" ]; then
              echo "resume: launcher back (try $n)"
              break
            fi
            echo "resume: attempt $n did not stick"
          done
        fi
        sleep 5
      done
    ) >>$L 2>&1 &

    # keep httpd alive while the flag is set, stop it when the flag goes away
    (
      while [ -f /conf/launcher_enable ]; do
        if ! pidof httpd >/dev/null 2>&1; then
          httpd -p 8181 -h /buffer/launcher -f >>$L 2>&1 &
          echo "launcher: httpd respawned at $(date)"
        fi
        sleep 20
      done
      p=$(pidof httpd 2>/dev/null)
      [ -n "$p" ] && kill $p 2>/dev/null && echo "launcher: httpd stopped (flag removed)"
    ) >>$L 2>&1 &
  fi

  echo "services done, id=$(id)"
} >>$L 2>&1

# if a USB stick shows up, drop a copy of the log on it for feedback
(
  i=0
  while [ $i -lt 40 ]; do
    for d in /media/sda1 /media/sdb1 /media/sda /media/sdb; do
      if [ -d "$d" ]; then
        cp -f $L "$d/tvhack.log" 2>/dev/null && exit 0
      fi
    done
    sleep 1
    i=$((i+1))
  done
) &

# --- self-heal: force the network permission on, whatever the privacy UI says ---
# (disabled by creating /conf/no_force_net)
if [ ! -f /conf/no_force_net ]; then
  {
    SQLITE=/conf/sqlite3
    [ -x "$SQLITE" ] || SQLITE=/buffer/sqlite3
    if [ -x "$SQLITE" ]; then
      i=0
      while [ $i -lt 10 ]; do
        "$SQLITE" /conf/sqlite_db/settingsDB.db \
          "UPDATE Settings SET m_privacy_internet_connection_en=1;" 2>/dev/null && break
        sleep 1
        i=$((i+1))
      done
      echo "db internet flag forced: $("$SQLITE" /conf/sqlite_db/settingsDB.db 'select m_privacy_internet_connection_en from Settings;' 2>/dev/null)"
    else
      echo "sqlite3 not found, db flag not forced"
    fi

    # Samba TV (ACR/CID) bypasses /etc/hosts with its own resolver -> block by IP
    if [ -x /vendor/iptables ]; then
      /vendor/iptables -C OUTPUT -d 216.183.117.0/24 -j REJECT 2>/dev/null || \
        /vendor/iptables -I OUTPUT -d 216.183.117.0/24 -j REJECT 2>/dev/null
    fi

  } >>$L 2>&1
fi

# Independent of vendor aurora and the native UI. Apply privacy before UI startup,
# then monitor both recovery listeners, blocks and Ethernet for the rest of boot.
if [ -x "$S/keepalive.sh" ]; then
  "$S/keepalive.sh" --once >>"$L" 2>&1
  "$S/keepalive.sh" >>"$L" 2>&1 < /dev/null &
fi

# Last kernel-ring snapshot before handing over; /buffer survives recovery.
dmesg > /buffer/tvhack-dmesg.log 2>/dev/null || true

# Hand the native branch to the staged boot entry. It re-checks the boot gate,
# starts the MCU watchdog feeder, records the attempt, selects
# openaurora-stageN.conf, and only then execs oa-supervise --wdt-keep.
if [ "$OA_NATIVE" = 1 ]; then
  exec /buffer/openaurora/run-aurora.sh >>"$L" 2>&1
fi
exec /vendor/com.nvt.idtv/aurora.elf
