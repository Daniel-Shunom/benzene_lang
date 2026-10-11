# Benzene direct-source versus structured-generation pilot

Eight tasks, one independent trial per arm; gpt-6.1-sol, low reasoning effort.
A emits direct source for check_program. B emits ordinary JSON arguments for generate_program.
Both receive the complete grammar, three examples, and the corresponding actual tool input schema.
The harness dispatches the real MCP calls and returns the full server response.
At most three repairs are allowed; both modes acknowledge the final response.
Codex-reported token usage includes all completions, including repairs if present.
Input includes cached tokens; reasoning tokens are a subset of output. Neither is added twice.
Fixed Codex instructions and unabridged JSON-RPC tool responses are included.
Each completion is fresh and receives the complete trial history; no constrained JSON decoding.

All 16 candidates passed on the first attempt. Independent source review passed all tasks;
this does not establish runtime behavior. See review_evidence in results.json.
All final sources were additionally checked with the standalone ether executable.
This is a small pilot with a ceiling effect; no general reliability advantage is established.

Reproduce using existing Codex login, Node, Erlang and compiled compiler/MCP modules:
python run_experiment.py --root PATH_TO_BENZENE --output OUTPUT_FOLDER --codex-js PATH_TO_CODEX_JS

The per-trial directories retain prompts, raw CLI events with usage, candidates, complete
MCP reports, and summaries. tasks.json contains the predeclared tasks and compiler-checked
reference sources; those references were not supplied to either model arm.
