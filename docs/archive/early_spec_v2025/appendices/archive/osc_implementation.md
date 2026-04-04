# OSC Implementation Reference

## Core Addresses
- **Transport**
  * /transport/play
  * /transport/stop
  * /transport/tempo     # Float: BPM
  * /transport/sync      # Boolean: external sync

- **Page Control**
  * /page/mode            # Int: 1=single, 2=dual
  * /page/swap            # Trigger: swap pages
  * /page/focus/{1-2}     # Int: focus page
  * /page/export          # String: export path
  * /page/import          # String: import path

- **Tab Control**
  * /pane/{1-2}/tab/{0-7}  # Int: select tab
  * /pane/{1-2}/grid/*     # Grid commands for specific pane

### Resource Management
- **Resource Control**
  * /resource/threshold            # Set resource thresholds
    - Arguments:
      * warning (f): 0.0-1.0 (default 0.8)
      * critical (f): 0.0-1.0 (default 0.9)
      * recovery (f): 0.0-1.0 (default 0.7)
  * /resource/voice/pool          # Voice pool status
  * /resource/voice/allocate      # Request voice allocation
  * /resource/voice/release       # Release voice allocation

[... continue with all OSC commands from external_control.md] 