# Compiler-feedback-only control

Run run_on_demand.py --root PATH --output PATH --codex-js PATH --no-docs.
Eight tasks, one trial per arm; gpt-6.1-sol, low reasoning effort.
A gets only check_program and its source-input schema. B gets only generate_program
and its structured-program schema. No grammar or examples are supplied to either.
Documentation retrieval is disabled, not simply omitted from the initial context.
No browsing, file access, shell commands, delegation or parent conversation.

An external agent loop executes chosen actions against the actual local MCP server.
Agents can discover their respective tool schemas, submit programs, repair using
feedback, and finish. Four submissions maximum, twelve total actions maximum.
Fresh CLI completions replay each trial's history. All reported input/output usage
is summed; cached input and reasoning are not added twice. Complete initial prompts,
per-action prompts, CLI events, tool calls, feedback and final candidates are retained.

This condition probes the information supplied by a structured generator when
language documentation is unavailable. It does not supersede the stronger practical
baseline that allows direct generation to consult documentation. Failed trials have
bounded costs; token totals are not costs for equally successful outputs.
No runtime execution; successful programs independently source-reviewed and checked.
