# CEC transport evidence

Interpretation: ../../reports/CEC-TRANSPORT-CONTRACT-2026-10-31.md.
All analysis/tests were offline. No TV access or device operations.

- transport-lifecycle, aurora-ice-interface, aurora-open-handler: Aurora wrappers,
  queues and ICE receive handling, with instructions. monitor-assembly is exact
  Thumb code for the unanalysed monitor at 0x88ba30.
- ndalex-cec, v4l2-dispatch, v4l2-result: NDAL control shapes, return values and
  lost Process_Command driver-result byte.
- kernel-cec-driver, kernel-cec-controls, kernel-cec-queue-pm, kernel-cec-ioctl:
  target driver ownership, raw/VPL packets, queues, close/reopen and PM paths.
- descriptor-initial-value, driver-command-table: exact ELF bytes, with file
  offsets/source identities explained in the report.
- host-packet, sanitize-packet, arm-packet-build, arm-packet: four packet groups;
  host-packet also reruns earlier nine CEC/audio groups. host-full: full host suite.

Two early query logs retain partial failures: kernel-cec requests a nonexistent
KER_CEC_Close after successfully dumping KER_CEC_Open; aurora-receive-loop fails
to find a Ghidra function at 0x88ba30. Later logs use exact existing symbols or
objdump. Headless exit zero does not prove a script completed; inspect script errors.
The @db1d8 query in kernel-cec-controls resolves inside DRV_CEC_Open and duplicates
that function; the actual close-part query is @dafd8 in kernel-cec-queue-pm.
