# CarsonBrowse

CarsonBrowse is CarsonOS's native browser project, not Firefox or a Firefox rebrand.

## Implemented foundation

- Bounded HTTP URL parser (HTTP only; explicit ports and query paths).
- HTTP/1.x response-header parser.
- Basic HTML-to-text extraction, title capture, common named entities, and script/style suppression.
- Back/forward history with 16 entries.
- A transport callback API so a future CarsonNet socket layer can fetch pages without mixing I/O into parsers.
- Host regression tests: run make carsonbrowse-check.

The existing desktop menu entry and carsonbrowse shell command remain the OS-facing launcher/status hook.

## Current limitations

This is browser-engine groundwork, not a live browser yet. CarsonOS does not currently provide a working TCP/IP socket API, DNS resolver, HTTPS/TLS, user-mode process isolation, or a browser window/address bar. No live site can be fetched from the ISO until a real transport is connected to cb_fetch_fn.

## Next implementation stages

1. Finish the E1000 driver and safely map its MMIO BAR.
2. Add packet buffers, ARP, IPv4, ICMP, UDP, TCP, DNS, and a bounded socket API in CarsonNet.
3. Connect plain HTTP transport to cb_fetch_fn; add timeouts and redirect limits.
4. Add CarsonTLS with certificate-chain and hostname validation. Never silently downgrade HTTPS to HTTP.
5. Add a framebuffer window with address bar, page text, links, scrolling, back, forward, and reload.
6. Expand HTML parsing and add a small CSS layout engine.
7. Add image decoding; consider JavaScript only later through an isolated maintained engine.

## Security boundaries

- Treat all downloaded content as untrusted.
- Keep parser buffers bounded.
- Never execute downloaded scripts in kernel mode.
- Do not label a connection secure without successful TLS certificate validation.
- Move browser parsing into isolated userland once CarsonOS has process isolation. The callback API is groundwork, not permission to parse hostile content in ring 0.
