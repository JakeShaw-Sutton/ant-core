# C3 dashboard transfer scheduling

The C3 shares its limited heap between BLE, Wi-Fi, TCP buffers and dashboard data. Full WebSocket status messages are roughly 6KB, so producing new copies during concurrent asset downloads or blackbox exports can exhaust networking memory.

`BoundedFileResponse` counts valid active file transfers. On the C3, full WebSocket telemetry waits while that count is nonzero. The count is released when a response is destroyed, including cancellation and disconnect. Missing files and failed buffer allocation do not acquire a count. Fresh status broadcasts resume after the last file response closes.

This rule only defers creation of full telemetry snapshots. Already queued WebSocket frames continue to drain and retry. Incoming controls, the application and safety loop, log messages, control acknowledgements and HTTP APIs continue to run. The dashboard can briefly show older telemetry while a bulk download finishes.

WebSocket fragments remain bounded at 2KB with TCP_NODELAY enabled. New C3 fragments require memory headroom; transient TCP output failures retry the already queued bytes without duplicating them. HTTP transfers retain their separate 1KB window and memory checks. The S3 does not defer telemetry during file transfers.
