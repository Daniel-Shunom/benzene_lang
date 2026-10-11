"""Reproduce the visible-text counts: pip install tiktoken==0.14.0; python count_tokens.py"""
import json
from pathlib import Path
import tiktoken

root = Path(__file__).resolve().parent
results = json.loads((root / "results.json").read_text(encoding="utf-8"))
encoding = tiktoken.get_encoding(results["encoding"])
for row in results["rows"]:
    text = (root / row["file"]).read_text(encoding="utf-8")
    measured = len(encoding.encode(text))
    assert measured == row["tokens"], (row["file"], measured, row["tokens"])
    print(f"{row['representation']}: {measured} tokens")
assert (root / "direct.bz").read_bytes() == (root / "generated.bz").read_bytes()
print("Counts reproduced; direct and MCP-generated source match byte for byte.")
