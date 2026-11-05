# Read-only testtool/EASI probe — 2026-11-05

The runtime EASI probe is implemented and tested against a local fixture, but
the TV's testtool server is disabled. No vendor process was paused, restarted
or modified.

## Firmware path

`FROST_DebugStartConsole` starts a testtool TCP server when
`devprofile/TesttoolSocket` is enabled. The socket task binds TCP **1986**
(`movw r0,#1986; ICE_SocketBind/Listen`).

Two read-only commands were recovered:

- `GETCHANNELLIST`: `%d-` RSN followed by the EASI name with control bytes
  stripped, records separated by `;`.
- `ACTIVECHANNELLIST`: XML
  `<active_list network='...' count='N' total_size='...'>` containing
  `<service rsn='...' name='...' handle='...' onid='...' tsid='...'
  sid='...' type='...'/>`.
- `GETCHANNELLISTVIEW` exists but writes view diagnostics; it is not used by
  the probe.

`RemoteDiagnosticCommandHandler::getChannelList` (`0xe5d445`) confirms the
RSN/name selection and control-byte stripping.

## Probe client

`openaurora/tools/probe/easi-probe.c`:

- connects to a configured host/port (default `127.0.0.1:1986`);
- accepts only `ACTIVECHANNELLIST` and `GETCHANNELLIST`; setters/debug commands
  are rejected before opening a socket;
- bounds the response to 256 KiB and requires an `</active_list>` terminator
  for the XML command;
- prints active-list network/count and `rsn/name/handle/onid/tsid/sid/type`
  rows.

The local fixture test verifies the exact `ACTIVECHANNELLIST\n` request, XML
parsing and rejection of `SETCHANNEL`.

## TV result

Read-only checks:

```
netstat: no listener on TCP 1986
is_testtool_socket_enabled @0x1796350 = 0
127.0.0.1:1986 -> Connection refused
LAN port 1986 -> filtered
```

The live `g_testtool`/object pointers are readable but were not used to bypass
the disabled command server. Enabling the server requires either a profile
change plus vendor-Aurora restart, or pausing/executing in the live process.
Both would stop/perturb vendor Aurora under its active MCU watchdog, and
physical USB recovery is unavailable. That step was not taken.

## Verification

```
ok easi probe: read-only ACTIVECHANNELLIST fixture and setter refusal
```

Full host `make check`: **411 ok**. `verify.cjs`: **45 groups**.

A fresh ARM build of the probe also succeeds:
`openaurora/build-tv-v3/easi-probe/easi-probe`.

## Limits

- No live runtime EASI data yet: testtool is disabled.
- `ACTIVECHANNELLIST` provides RSN/name/IDs/type, not the active LCN or
  visibility/HD-simulcast flags. `ApplicationServiceList::getLcn` and
  `isHiddenService` still need a runtime call path.
- The provisional channel UI remains on stored data.
- No TV state was modified; no process was paused or restarted.
