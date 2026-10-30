# RecoverySuite handoff

The active checkpoint is the 2026-10-30 recovery-results handoff, resumed from Phase 15A commit `c8475ee`.
Read README.md, SESSION_STATE.md and MASTER_TODO.md for current facts; older root-level completion
notes describe historical claims and can be stale.

Build using the README commands. CTest has 23 executables; the new QtTest target tests results,
plain-text handling, unknown statistics, service failures and cancellation completion.

Start the next implementation with a read-only image source and integrate it into the GUI, then
safe filesystem export. GUIRecoveryService currently accepts an injected reader but does not bind
`devicePath` to a new device; do not treat arbitrary device names as independent opened sources.
Output paths are requested destinations only. Backend buffers are not currently exported or retained
as file contents in the results model, which exposes names and sizes only.

Cancellation is cooperative at service boundaries, not inside backend loops. The worker flag is atomic
and completion/cleanup now occurs for cancellation too. Avoid adding queued cancellation slots that
cannot execute while the worker event loop is busy.

The Linux CI workflow validates Debug and Release. Check its actual remote result before merging;
no Windows or real-hardware validation has been completed in this checkpoint.
