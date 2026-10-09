"""Check structured generation against the repository's actual front end."""
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    compiler = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "bin" / "ether.exe"
    generated = subprocess.run(
        ["gleam", "run", "-m", "generated_examples"],
        cwd=ROOT / "mcp" / "ether_mcp", capture_output=True, text=True, check=True,
    )
    examples = json.loads(generated.stdout)
    for example in examples:
        source = example["source"].encode("utf-8")
        result = subprocess.run(
            [str(compiler.resolve()), "scan", example["name"] + ".bz", "-stdin"],
            input=str(len(source)).encode("ascii") + b"\n" + source,
            capture_output=True, check=True,
        )
        report = json.loads(result.stdout)
        assert not report["diagnostics"], (example["name"], report["diagnostics"], example["source"])
        assert report["index"], example["name"]
        if example["name"] == "expressions":
            strings = [t["value"] for t in report["tokens"] if t["type"] == "StringLiteral"]
            assert strings == ['quoted " slash \\ newline\n tab\t return\r null\0'], strings
        print("PASS", example["name"])
    print(f"{len(examples)} generated programs passed lexer, parser, resolution and type checking")


if __name__ == "__main__":
    main()
