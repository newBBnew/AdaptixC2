# WebSocket Auxiliary Module Stub

This directory hosts the build script and placeholder implementation for the
HTTP Beacon WebSocket auxiliary channel. The root Makefile automatically builds
this module during `make extenders` (if the MinGW cross toolchain is available)
and copies the resulting DLL to the distribution (`release/extenders/...`) as
well as the runtime tree (`extenders/...`).

Build manually:

```bash
cd Extenders/agent_beacon/modules/wsclient
make
```

Replace the stub implementation with the real WebSocket client logic once it is
ready.
