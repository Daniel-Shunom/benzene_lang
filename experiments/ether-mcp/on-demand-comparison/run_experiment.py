"""Paired direct-source/structured-generation pilot using Codex and real MCP calls.

Run with existing Codex login, Node, Erlang and built Benzene compiler/server:
python run_experiment.py --root PATH --output PATH --codex-js PATH
No constrained decoding, model-generated shell commands, or model delegation.
Each model completion is a fresh CLI session replaying the complete trial history.
"""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time
import urllib.request

MODEL = 'gpt-6.1-sol'
TASKS = [
    ('addition', 'Define add(x: Int, y: Int) returning their sum, and main() returning add(20, 22).',
     'func add(x: Int, y: Int) :> Int\n x + y\nend\nfunc main() :> Int\n add(20, 22)\nend'),
    ('grouping', 'Define scaled_sum(a: Int, b: Int, c: Int) returning (a + b) multiplied by c. Define main() calling it with 2, 3, 4. Parentheses here describe arithmetic, not target syntax.',
     'func scaled_sum(a: Int, b: Int, c: Int) :> Int\n { a + b } * c\nend\nfunc main() :> Int\n scaled_sum(2, 3, 4)\nend'),
    ('call_arguments', 'Define square(x: Int) returning x multiplied by itself. Define main() returning square applied to the sum of 2 and 3, plus square applied to the sum of 4 and 5. Put each addition directly in the corresponding call argument, without intermediate bindings.',
     'func square(x: Int) :> Int\n x * x\nend\nfunc main() :> Int\n square({ 2 + 3 }) + square({ 4 + 5 })\nend'),
    ('boolean', 'Define eligible(age: Int, blocked: Bool) returning true exactly when age is at least 18 and blocked is false. Define main() returning eligible(21, False). Use logical negation and conjunction in eligible.',
     'func eligible(age: Int, blocked: Bool) :> Bool\n { age >= 18 } && { ~blocked }\nend\nfunc main() :> Bool\n eligible(21, False)\nend'),
    ('collections', 'Define numbers() returning the integer list [1, 2, 3], and pair() returning a tuple containing integer 7 and string seven. Define main() returning pair(). Use explicit return types on all functions.',
     'func numbers() :> List(Int)\n [1, 2, 3]\nend\nfunc pair() :> Tuple(Int, String)\n @{7, "seven"}\nend\nfunc main() :> Tuple(Int, String)\n pair()\nend'),
    ('string_escaping', 'Define message() returning exactly this string: first line is He said "hello"; second line is C:\\temp\\file. There is one newline between the lines and no final newline. Define main() returning message(). Use explicit String return types.',
     'func message() :> String\n "He said \\"hello\\"\\nC:\\\\temp\\\\file"\nend\nfunc main() :> String\n message()\nend'),
    ('case', 'Define classify(x: Int) returning string zero for integer 0, one for integer 1, and other for every other integer, using a case expression with a catch-all branch. Define main() returning classify(2). Use explicit return types.',
     'func classify(x: Int) :> String\n case x:\n 0 :> "zero"\n 1 :> "one"\n _ :> "other"\n end\nend\nfunc main() :> String\n classify(2)\nend'),
    ('lambda', 'Define apply_twice(f: Fn(Int) :> Int, x: Int) returning f(f(x)). Define main() binding increment to an anonymous function that takes n: Int and returns n + 1, then returning apply_twice(increment, 40). Use explicit return types for named and anonymous functions.',
     'func apply_twice(f: Fn(Int) :> Int, x: Int) :> Int\n f(f(x))\nend\nfunc main() :> Int\n let increment = Fn(n: Int) :> Int\n n + 1\n end\n apply_twice(increment, 40)\nend'),
]
EXAMPLES = '''func identity(x: Int) :> Int
  x
end

func double(x: Int) :> Int
  x * 2
end

func main() :> Int
  let value: Int = double(5)
  identity(value)
end'''


def compact(value):
    return json.dumps(value, ensure_ascii=False, separators=(',', ':'))


def save(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8', newline='\n')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--codex-js', type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    work = out / 'empty-workspace'
    work.mkdir(exist_ok=True)
    grammar = (root / 'docs/grammar.md').read_text(encoding='utf-8')
    (out / 'grammar.md').write_text(grammar, encoding='utf-8', newline='\n')
    (out / 'examples.bz').write_text(EXAMPLES, encoding='utf-8', newline='\n')
    save(out / 'tasks.json', [{'id': n, 'prompt': p, 'reference_source': s} for n,p,s in TASKS])
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    libs = root / 'mcp/ether_mcp/build/dev/erlang'
    exe = root / 'bin/ether.exe'
    flags = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    command = [shutil.which('erl'), '-noshell', '-pa'] + [str(p) for p in libs.glob('*/ebin')]
    command += ['-eval', "'mcp@server':main()."]
    server = subprocess.Popen(command, cwd=root / 'mcp/ether_mcp',
        env=dict(os.environ, BENZENE_MCP_PORT=str(port), ETHER_BIN=str(exe)),
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **flags)

    def rpc(method, params=None):
        request = {'jsonrpc': '2.0', 'id': 'pilot', 'method': method}
        if params is not None:
            request['params'] = params
        req = urllib.request.Request(f'http://127.0.0.1:{port}/mcp',
            data=compact(request).encode('utf-8'), headers={
                'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream',
                'MCP-Protocol-Version': '2025-11-25'})
        with urllib.request.urlopen(req, timeout=30) as response:
            return json.load(response)

    def completion(prompt, folder, index):
        (folder / f'{index:02d}.prompt.txt').write_text(prompt, encoding='utf-8', newline='\n')
        cmd = [shutil.which('node'), str(args.codex_js), 'exec', '--ignore-user-config',
            '--ephemeral', '--skip-git-repo-check', '--json', '-s', 'read-only',
            '-m', MODEL, '-c', 'model_reasoning_effort="low"', '-C', str(work), '-']
        started = time.monotonic()
        run = subprocess.run(cmd, input=prompt, capture_output=True, text=True,
            encoding='utf-8', errors='replace', timeout=240, **flags)
        (folder / f'{index:02d}.events.jsonl').write_text(run.stdout, encoding='utf-8', newline='\n')
        (folder / f'{index:02d}.stderr.txt').write_text(run.stderr, encoding='utf-8', newline='\n')
        events = [json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')]
        usage = [e['usage'] for e in events if e['type'] == 'turn.completed']
        if run.returncode or not usage:
            raise RuntimeError(f'Model request failed in {folder}: {run.stderr[-1000:]} {run.stdout[-1000:]}')
        items = [e['item'] for e in events if e['type'] == 'item.completed']
        violations = [i['type'] for i in items if i['type'] not in ('agent_message', 'reasoning')]
        if violations:
            raise RuntimeError(f'Unexpected model-side tool use: {violations}')
        message = [i['text'] for i in items if i['type'] == 'agent_message'][-1]
        return message, usage[-1], time.monotonic() - started

    def trial(task, arm, schema):
        name, description, _ = task
        folder = out / f'{name}-{arm}'
        folder.mkdir(exist_ok=True)
        instructions = ('Write only raw Benzene source. The harness will submit it to check_program.' if arm == 'A'
            else 'Write only a JSON object containing the generate_program arguments. The harness will call generate_program with compiler validation enabled. Use the supplied input schema. No raw-source escape hatches.')
        prompt = f'''You are participating in a controlled programming experiment. Do not use any tools, read files, browse, or ask questions. Everything needed is in this prompt. Output no explanations or Markdown fences.
TASK: {description}
MODE {arm}: {instructions}
Both modes get at most four submitted candidates (initial attempt plus three repairs). After each candidate the harness returns the actual tool result. If it is compiler-valid and satisfies the task, reply only DONE; otherwise output a complete revised candidate. You must first submit a candidate, not DONE.

GRAMMAR AND RESTRICTIONS:
{grammar}

EXAMPLES:
{EXAMPLES}

AVAILABLE TOOL INPUT SCHEMA:
{compact(schema)}
'''
        attempts, usages, elapsed = [], [], 0
        done = False
        for step in range(5):
            message, usage, duration = completion(prompt, folder, step)
            usages.append(usage)
            elapsed += duration
            if message.strip() == 'DONE':
                if not attempts or not attempts[-1]['compiler_accepted']:
                    raise RuntimeError(f'Premature DONE: {name}-{arm}')
                done = True
                break
            if step == 4:
                break
            candidate = message.strip()
            if candidate.startswith('```'):
                candidate = '\n'.join(candidate.splitlines()[1:-1])
            try:
                arguments = {'source': candidate} if arm == 'A' else json.loads(candidate)
                if arm == 'B':
                    arguments['validate'] = True
                response = rpc('tools/call', {'name': 'check_program' if arm == 'A' else 'generate_program', 'arguments': arguments})
                report = response.get('result', {}).get('structuredContent', {})
            except (ValueError, TypeError) as error:
                response = {'error': {'message': 'Invalid JSON arguments: ' + str(error)}}
                report = {}
            source = candidate if arm == 'A' else report.get('source', '')
            accepted = bool(report.get('validated') and report.get('valid'))
            attempt = {'compiler_accepted': accepted, 'source': source,
                       'candidate': candidate, 'tool_response': response}
            attempts.append(attempt)
            save(folder / f'{step:02d}.validation.json', response)
            (folder / f'{step:02d}.candidate.txt').write_text(candidate, encoding='utf-8', newline='\n')
            if source:
                (folder / f'{step:02d}.bz').write_text(source, encoding='utf-8', newline='\n')
            prompt += '\nPREVIOUS CANDIDATE:\n' + message + '\nACTUAL TOOL RESULT:\n' + compact(response)
            prompt += '\nReply DONE if compiler-valid and task-correct; otherwise submit the complete repaired candidate. No explanations.\n'
            print(f'{name}-{arm}: candidate {step+1}, accepted={accepted}', flush=True)
        totals = {key: sum(u.get(key, 0) for u in usages)
            for key in ('input_tokens', 'cached_input_tokens', 'cache_write_input_tokens', 'output_tokens', 'reasoning_output_tokens')}
        totals['total_tokens'] = totals['input_tokens'] + totals['output_tokens']
        result = {'task': name, 'arm': arm, 'model': MODEL, 'reasoning_effort': 'low',
            'compiler_accepted': bool(attempts and attempts[-1]['compiler_accepted']),
            'first_attempt_accepted': bool(attempts and attempts[0]['compiler_accepted']),
            'repair_rounds': max(0,len(attempts)-1), 'model_completions': len(usages),
            'acknowledged_final_feedback': done, 'usage': totals, 'elapsed_seconds': elapsed,
            'attempts': attempts, 'per_completion_usage': usages,
            'task_correctness': 'pending independent source review'}
        save(folder / 'result.json', result)
        print(f'FINISHED {name}-{arm}: {totals["total_tokens"]} total tokens, {result["repair_rounds"]} repairs', flush=True)
        return result

    results = []
    try:
        deadline = time.monotonic() + 25
        while True:
            try:
                rpc('ping')
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.2)
        rpc('initialize', {'protocolVersion': '2025-11-25', 'capabilities': {},
                          'clientInfo': {'name': 'benzene-ab-pilot', 'version': '1'}})
        tools = rpc('tools/list')['result']['tools']
        save(out / 'tools.json', tools)
        schemas = {t['name']: t['inputSchema'] for t in tools}
        for name, description, source in TASKS:
            response = rpc('tools/call', {'name':'check_program', 'arguments':{'source':source}})
            save(out / f'{name}.reference-validation.json', response)
            report = response['result']['structuredContent']
            if not report.get('valid'):
                raise RuntimeError(f'Invalid task reference {name}: {compact(report)}')
        print('All eight task references pass compiler validation.', flush=True)
        # Alternate A/B order; each task and arm receives fresh context.
        # Independent paired completions run together to reduce wall time.
        for index, task in enumerate(TASKS):
            arms = ['A','B'] if index % 2 == 0 else ['B','A']
            with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
                futures = [pool.submit(trial, task, arm, schemas['check_program' if arm == 'A' else 'generate_program']) for arm in arms]
                for future in futures:
                    results.append(future.result())
            save(out / 'results.json', {'model': MODEL, 'results': results,
                'method': '8 tasks, 1 run per arm; fresh CLI completion with full history replay each turn; final tool feedback acknowledged; actual Codex usage; no constrained decoding; harness-dispatched real MCP calls.',
                'reasoning_tokens_are_subset_of_output': True})
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()


if __name__ == '__main__':
    main()
