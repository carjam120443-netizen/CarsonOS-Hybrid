# CarsonBrowse

CarsonBrowse is the planned native web browser for CarsonOS. It is our own project and is not Firefox or a Firefox rebrand.

## Current status

The current kernel integration is a desktop-menu entry and a `carsonbrowse` shell status command. **There is no working browser engine yet.** This scaffold does not fetch or render websites.

## Planned implementation

Build this in small native components instead of trying to port a desktop browser immediately:

1. CarsonNet: Ethernet driver, packet buffers, ARP, IPv4, ICMP, UDP, TCP, DNS, and sockets.
2. CarsonTLS: certificate validation and HTTPS support.
3. CarsonHTML: a safe HTML tokenizer and basic document tree.
4. CarsonCSS: a deliberately small layout/style subset.
5. CarsonRender: text, links, images, scrolling, and framebuffer drawing.
6. CarsonBrowse UI: address bar, back/forward, reload, tabs later, and page view.
7. JavaScript support only after the core browser is stable; use a well-isolated engine rather than improvising an unsafe partial implementation.

## Initial scope

The first usable milestone should be a text-focused HTTP browser for simple pages. HTTPS, CSS layout, images, and JavaScript can follow in separate milestones.

## Safety and engineering notes

- Treat all downloaded page content as untrusted input.
- Bound every network buffer and parser allocation.
- Validate TLS certificates before showing a connection as secure.
- Do not execute page scripts in kernel mode.
- Keep the browser engine out of the kernel; it needs user-mode process isolation first.
