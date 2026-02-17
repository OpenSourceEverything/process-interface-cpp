# Design V1 Notes

## Canonical Contract
1. Primary contract document:
- `docs/design/contract-v0.md`
2. Bridge/40318 mapping document:
- `docs/design/compat-bridge-40318-v0.md`
3. This file remains design context and rationale notes.
4. If examples below use shorthand keys (`m`, `p`, `r`, `evt`), treat them as legacy notes only.
5. For implementation, use readable V0 keys: `method`, `params`, `response`, `error`, `event`.

robust, platform agnostic, minimal app mods, gui from schema

best fit: stdio json-rpc + schema-driven ui

a) process layout
  1. app process = your real app
  2. gui process = generic gui host you ship once
  3. link = pipes (stdio) between them

b) app modification
  1. add a tiny "rpc adapter" module
     - reads json lines from stdin
     - writes json lines to stdout
     - calls your internal functions
  2. expose 3 required rpc methods
     - schema.get
     - state.get
     - action.invoke

c) message protocol (newline delimited json)
  request:
    {"id":1,"m":"schema.get","p":{}}
  reply:
    {"id":1,"ok":true,"r":{...schema...}}

  request:
    {"id":2,"m":"state.get","p":{}}
  reply:
    {"id":2,"ok":true,"r":{...current state...}}

  request:
    {"id":3,"m":"action.invoke","p":{"name":"save","args":{...}}}
  reply:
    {"id":3,"ok":true,"r":{"result":...}}

  async event (no id) from app to gui:
    {"evt":"state.changed","p":{"path":"motor.rpm","value":1234}}

d) schema shape (minimum useful)
  - version
  - title
  - fields (controls)
  - actions (buttons / commands)
  - validation rules

example schema (small)
  {
    "version":1,
    "title":"device",
    "fields":[
      {"path":"motor.rpm","type":"int","label":"rpm","min":0,"max":6000},
      {"path":"motor.enable","type":"bool","label":"enable"},
      {"path":"net.ip","type":"string","label":"ip","pattern":"^([0-9]{1,3}\\.){3}[0-9]{1,3}$"}
    ],
    "actions":[
      {"name":"save","label":"save"},
      {"name":"reboot","label":"reboot","confirm":"reboot now?"}
    ]
  }

e) robustness additions (still simple)
  1. framing
     - newline delimited is ok if you guarantee no embedded newlines
     - enforce: json must be one line (no pretty print)
  2. timeouts
     - gui sets per-call timeout, shows error, allows retry
  3. backpressure
     - gui reads continuously so app cannot block on stdout
  4. heartbeats
     - gui sends {"id":X,"m":"ping"} every N seconds
     - app replies ok; gui shows "disconnected" if missed
  5. versioning
     - schema.get returns protocol_version + schema_version
     - gui refuses unknown major protocol_version
  6. error contract
     - reply error shape:
       {"id":9,"ok":false,"e":{"code":"E_BAD_ARG","msg":"rpm out of range"}}
  7. security boundary (basic)
     - if untrusted: add auth token in every request
       {"id":1,"m":"...","p":{...},"auth":"..."}
     - for local trusted tools you can skip

f) minimal app side adapter contract
  - must provide stable state tree (json object)
  - must support setting values
    method: state.set
    request: {"id":4,"m":"state.set","p":{"path":"motor.rpm","value":1200}}
    reply:   {"id":4,"ok":true,"r":{}}

g) gui generation rule of thumb
  type bool   -> checkbox
  type int    -> numeric input + slider if min/max present
  type float  -> numeric input
  type string -> text input
  enum list   -> dropdown
  group/path  -> panels / tabs based on path prefixes

h) why this is the best simplest robust choice
  - platform agnostic: stdio exists everywhere
  - minimal app change: one adapter module, no gui code in app
  - modular: gui host reused for every app
  - schema-driven: ui generated, not hand built

i) tight spec you can implement fast
  required methods: schema.get, state.get, state.set, action.invoke, ping
  optional events: state.changed, log
