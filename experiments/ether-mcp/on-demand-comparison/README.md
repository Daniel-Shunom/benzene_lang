# Fresh agents learning Benzene on demand

Run run_on_demand.py with --root, --output, --codex-js; see --help.
The default external text-action runner does not need the Codex code-mode host.
Each independent trial begins with English task requirements and an action protocol.
No grammar, examples, tool input schemas or parent conversation are provided initially.
The agent chooses discover_tools, read_language_docs, its actual compiler/generator tool,
and finish. The harness forwards submissions to the real local MCP server. Both modes
can request identical documents. The initial prompts and every completion are retained.

Up to 4 submissions and 12 actions per trial. Model: gpt-6.1-sol, low reasoning effort.
One trial per arm per task; no constrained decoding, shell, web, file access or delegation.
Total usage is summed across all completions, including discovery, document retrieval,
feedback and finish. Full trial history is replayed in each fresh completion.
Cached inputs and reasoning are subsets of input/output, not additional totals.

All results have independent source-review evidence and standalone compiler validation.
No programs were executed. Native-runner smoke tests were excluded because the local
tool-host binary was empty; these were infrastructure failures, not language failures.
The experiment cannot verify whether Benzene appeared in the model's pretraining.
