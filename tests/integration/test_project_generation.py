"""Test the CLI's scaffold, optional Git setup and actual front-end output."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"


def main():
    compiler = (Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "bin" / "ether.exe").resolve()
    BUILD.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="project-generation-", dir=BUILD) as directory:
        workspace = Path(directory).resolve()
        assert workspace.parent == BUILD.resolve(), "Unexpected test cleanup directory"
        # Spaces in the parent exercise native process argument quoting.
        work = workspace / "parent with spaces"
        work.mkdir()

        def run(*args, env=None):
            return subprocess.run([str(compiler), *args], cwd=work, env=env,
                                  capture_output=True, text=True, timeout=20)

        created = run("new", "my_project")
        assert created.returncode == 0, created.stderr
        project = work / "my_project"
        assert (project / "src" / "main.bz").read_text() == "func main() :> Nil\n  Nil\nend\n"
        assert (project / "README.md").read_text().startswith("# my_project\n")
        instructions = (project / "AGENTS.md").read_text()
        for required in ["tools/list", "generate_program", "check_program", "validate: true", "analysis.diagnostics"]:
            assert required in instructions, required
        config = json.loads((project / ".mcp.json").read_text())
        assert config["mcpServers"]["benzene"]["url"] == "http://127.0.0.1:4000/mcp"
        assert "build/" in (project / ".gitignore").read_text()
        if shutil.which("git"):
            assert (project / ".git").is_dir(), created.stdout
            top = subprocess.run(["git", "-C", str(project), "rev-parse", "--show-toplevel"],
                                 capture_output=True, text=True, check=True)
            assert Path(top.stdout.strip()).resolve() == project.resolve()
        checked = run("scan", str(project / "src" / "main.bz"))
        assert checked.returncode == 0, checked.stderr
        report = json.loads(checked.stdout)
        assert not report["diagnostics"], report["diagnostics"]
        assert any(e["name"] == "main" and e["isDefinition"] and e["returns"] == "Nil" for e in report["index"])
        assert run("check", str(project / "src" / "main.bz")).returncode == 0

        sentinel = project / "README.md"
        sentinel.write_text("keep my changes")
        assert run("new", "my_project").returncode != 0
        assert sentinel.read_text() == "keep my changes"
        existing_file = work / "existing_file"
        existing_file.write_text("keep this file")
        assert run("new", "existing_file").returncode != 0
        assert existing_file.read_text() == "keep this file"
        before = set(work.iterdir())
        for invalid in [".", "..", "../escape", "a/b", "a\\b", "bad name", "-option", "CON", "nul", "LPT1", "a;echo"]:
            assert run("new", invalid).returncode != 0, invalid
        assert set(work.iterdir()) == before
        assert run("new").returncode != 0
        assert run("new", "extra", "argument").returncode != 0
        assert run("create", "legacy").returncode == 0

        no_git_env = dict(os.environ, PATH=str(workspace / "empty-path"))
        no_git = run("new", "without_git", env=no_git_env)
        assert no_git.returncode == 0, no_git.stderr
        assert "Git is not available" in no_git.stdout
        assert not (work / "without_git" / ".git").exists()
        assert (work / "without_git" / "src" / "main.bz").is_file()

        if shutil.which("git"):
            failing_env = dict(os.environ, GIT_CONFIG_COUNT="1",
                               GIT_CONFIG_KEY_0="init.defaultBranch",
                               GIT_CONFIG_VALUE_0="invalid branch name")
            failed_git = run("new", "failed_git", env=failing_env)
            assert failed_git.returncode != 0
            assert "Git initialization failed" in failed_git.stderr
            assert (work / "failed_git" / "src" / "main.bz").is_file()
        assert "new" in run("help").stdout
        print("PASS project scaffold, Git setup/absence/failure, overwrite protection, CLI arguments and starter type checking")


if __name__ == "__main__":
    main()
