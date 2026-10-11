"""Fresh autonomous agents learning Benzene through MCP documentation on demand.

Same eight tasks as the upfront-documentation pilot, expressed without target syntax.
Each CLI session receives only its task and procedure. The real tools and a shared
documentation tool are exposed by a loopback proxy. Tool calls, usage and sources
are retained. No other model/agent receives this conversation's context.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import threading
import time
import urllib.request

from run_experiment import MODEL, TASKS, EXAMPLES, compact, save

PROMPTS = {
    'addition': 'Write a complete Benzene program with a function add taking two integers x and y and returning their sum. A zero-argument main function must return add applied to 20 and 22.',
    'grouping': 'Write a complete Benzene program with scaled_sum taking integers a, b and c, adding a and b first, then multiplying that result by c. A zero-argument main function must call scaled_sum with 2, 3 and 4.',
    'call_arguments': 'Write a complete Benzene program with square taking an integer x and returning x multiplied by itself. A zero-argument main must add square applied to the sum of 2 and 3 to square applied to the sum of 4 and 5. Put each addition directly inside its call argument, without intermediate bindings.',
    'boolean': 'Write a complete Benzene program with eligible taking an integer age and Boolean blocked, returning true exactly when age is at least 18 and blocked is false. Use logical negation and conjunction. A zero-argument main must call eligible with age 21 and blocked false.',
    'collections': 'Write a complete Benzene program with numbers returning a list containing integers 1, 2 and 3, and pair returning a tuple containing integer 7 and string seven. A zero-argument main must return pair applied to no arguments. Give all functions explicit return types.',
    'string_escaping': 'Write a complete Benzene program with message returning exactly a two-line string: the first line is He said "hello" and the second line is C:\\temp\\file. There is one newline between the lines and no final newline. A zero-argument main must return message applied to no arguments. Give all functions explicit string return types.',
    'case': 'Write a complete Benzene program with classify taking integer x and returning string zero for integer 0, one for integer 1, and other for every other integer. Use a case expression with a catch-all branch. A zero-argument main must return classify applied to 2. Give all functions explicit return types.',
    'lambda': 'Write a complete Benzene program with apply_twice taking an integer-to-integer function f and integer x, returning f applied twice to x. A zero-argument main must bind increment to an anonymous function taking integer n and returning n plus 1, then return apply_twice applied to increment and 40. Give all named and anonymous functions explicit return types.',
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--codex-js', type=Path, required=True)
    parser.add_argument('--tasks', type=int, default=8, help='Smoke-test with 1; full experiment uses 8.')
    parser.add_argument('--runner', choices=('harness','native'), default='harness')
    parser.add_argument('--no-docs', action='store_true', help='No language documentation in either mode; A has compiler feedback only, B has generator schema and feedback.')
    args = parser.parse_args()
    root, out = args.root.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    grammar = '' if args.no_docs else (root / 'docs/grammar.md').read_text(encoding='utf-8')
    docs = {'grammar': grammar, 'examples': EXAMPLES,
        'overview': 'Benzene is a statically typed functional language. The compiler checks syntax, name resolution and types. It cannot yet execute programs. The grammar and examples can be requested separately through this tool.',
        'all': grammar + '\n\nEXAMPLES:\n' + EXAMPLES}
    if args.no_docs:
        docs = {}
    save(out / 'documentation.json', docs)
    save(out / 'tasks.json', PROMPTS)
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    libs = root / 'mcp/ether_mcp/build/dev/erlang'
    flags = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    command = [shutil.which('erl'), '-noshell', '-pa'] + [str(p) for p in libs.glob('*/ebin')]
    command += ['-eval', "'mcp@server':main()."]
    server = subprocess.Popen(command, cwd=root / 'mcp/ether_mcp',
        env=dict(os.environ, BENZENE_MCP_PORT=str(port), ETHER_BIN=str(root/'bin/ether.exe')),
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **flags)

    def upstream(method, params=None):
        req = {'jsonrpc':'2.0','id':'trial','method':method}
        if params is not None:
            req['params'] = params
        http = urllib.request.Request(f'http://127.0.0.1:{port}/mcp', data=compact(req).encode(), headers={
            'Content-Type':'application/json','Accept':'application/json, text/event-stream',
            'MCP-Protocol-Version':'2025-11-25'})
        with urllib.request.urlopen(http, timeout=30) as response:
            return json.load(response)

    def trial(name, arm, tools):
        folder = out / f'{name}-{arm}'
        folder.mkdir(exist_ok=True)
        work = folder / 'empty-workspace'
        work.mkdir(exist_ok=True)
        calls = []
        permitted = 'check_program' if arm == 'A' else 'generate_program'
        exposed = [t for t in tools if t['name']==permitted] + ([] if args.no_docs else [{
            'name':'read_language_docs',
            'description':'Read Benzene documentation on demand. Both experimental modes have access to the same documents. Choose grammar, examples, overview, or all. No documents are included automatically.',
            'inputSchema': {'type':'object','properties':{'topic':{'type':'string','enum':list(docs)}},'required':['topic'],'additionalProperties':False},
        }])
        save(folder / 'exposed-tools.json', exposed)

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, *args):
                pass

            def do_GET(self):
                self.send_response(405)
                self.end_headers()

            def do_POST(self):
                request = json.loads(self.rfile.read(int(self.headers.get('Content-Length','0'))))
                method = request['method']
                response = {'jsonrpc':'2.0', 'id':request.get('id')}
                if method.startswith('notifications/'):
                    self.send_response(202)
                    self.end_headers()
                    return
                if method == 'initialize':
                    response['result'] = {'protocolVersion':'2025-11-25','capabilities':{'tools':{'listChanged':False}},'serverInfo':{'name':'benzene-on-demand-pilot','version':'1'}}
                elif method == 'ping':
                    response['result'] = {}
                elif method == 'tools/list':
                    response['result'] = {'tools':exposed}
                elif method == 'tools/call':
                    params = request['params']
                    tool = params['name']
                    arguments = params.get('arguments',{})
                    record = {'tool':tool,'arguments':arguments,'started':time.time()}
                    try:
                        if tool == 'read_language_docs' and not args.no_docs:
                            result = {'content':[{'type':'text','text':docs[arguments['topic']]}],'isError':False}
                        elif tool == permitted:
                            submissions = sum(c['tool']==permitted for c in calls)
                            if submissions >= 4:
                                result = {'content':[{'type':'text','text':'Submission budget exhausted (initial attempt plus three repairs). Stop and report failure.'}],'isError':True}
                                record['budget_exceeded'] = True
                            else:
                                forwarded = dict(arguments)
                                if arm == 'B':
                                    forwarded['validate'] = True
                                result = upstream('tools/call', {'name':tool,'arguments':forwarded})['result']
                        else:
                            result = {'content':[{'type':'text','text':'Tool unavailable in this experimental mode.'}],'isError':True}
                        response['result'] = result
                        record['result'] = result
                    except Exception as error:
                        response['error'] = {'code':-32603,'message':str(error)}
                        record['error'] = str(error)
                    calls.append(record)
                    save(folder / 'calls.json', calls)
                    if tool == permitted:
                        report = record.get('result',{}).get('structuredContent',{})
                        print(f'{name}-{arm}: submission, accepted={report.get("valid",False)}', flush=True)
                    else:
                        print(f'{name}-{arm}: docs {arguments.get("topic")}', flush=True)
                else:
                    response['error'] = {'code':-32601,'message':'Method not supported'}
                body = compact(response).encode()
                self.send_response(200)
                self.send_header('Content-Type','application/json')
                self.send_header('Content-Length',str(len(body)))
                self.end_headers()
                self.wfile.write(body)

        proxy = ThreadingHTTPServer(('127.0.0.1',0),Handler)
        thread = threading.Thread(target=proxy.serve_forever,daemon=True)
        thread.start()
        prompt = f'''You are an autonomous agent in a controlled programming experiment. Write a program in the Benzene programming language. You have received no Benzene grammar or examples in this prompt. Discover the tools and consult read_language_docs if needed; the documents are identical for both modes. Do not browse, use shell commands, inspect files, or delegate. Everything you need is accessible through this experiment's MCP server.

TASK: {PROMPTS[name]}

MODE {arm}: {'Write the program source yourself and submit the complete source to check_program.' if arm=='A' else 'Describe the complete program using generate_program. Compiler validation is enabled. Do not use raw-source escape hatches.'}

You may make at most four program submissions: an initial attempt and three repairs. Use compiler feedback to repair problems. Submit complete programs rather than individual fragments. Stop once the compiler accepts a complete program that satisfies the task. Then briefly report the result. If the budget is exhausted, report failure. Do not ask questions.
'''
        if args.no_docs:
            prompt = prompt.replace('Discover the tools and consult read_language_docs if needed; the documents are identical for both modes.',
                'No grammar, examples, language documentation, browsing or file access are available. Discover your mode\'s tool. In mode A, check_program accepts source and returns compiler feedback; it does not provide a grammar. In mode B, generate_program provides a structured input schema and validation feedback. Do not invent access to documentation or other sources of Benzene syntax.')
        (folder / 'initial-prompt.txt').write_text(prompt,encoding='utf-8',newline='\n')
        cmd = [shutil.which('node'),str(args.codex_js),'exec','--ignore-user-config','--ephemeral',
            '--skip-git-repo-check','--json','-s','read-only','-m',MODEL,
            '-c','model_reasoning_effort="low"','--disable','shell_tool','--disable','multi_agent',
            '--disable','code_mode_host',
            '-c','web_search="disabled"','-c',f'mcp_servers.benzene.url="http://127.0.0.1:{proxy.server_port}/mcp"',
            '-c','mcp_servers.benzene.required=true','-C',str(work),'-']
        started = time.monotonic()
        try:
            if args.runner == 'native':
                run = subprocess.run(cmd,input=prompt,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=360,**flags)
            else:
                # Text-action agent protocol avoids the broken local code-mode host.
                # The agent chooses its actions; no grammar/schema is injected upfront.
                prompt += '''\nUse ONLY this external agent protocol. Do not invoke Codex's built-in tools; the harness executes your requested action. Output exactly one ordinary JSON object, without fences, per response: {"tool": "NAME", "arguments": {...}}.
Available actions: discover_tools (arguments {}), read_language_docs (arguments {"topic":"grammar"}, {"topic":"examples"}, {"topic":"overview"}, or {"topic":"all"}), your mode's program tool, and finish (arguments {"summary":"..."}). discover_tools returns actual tool definitions and input schemas. No grammar, examples or schemas have been provided yet. Choose what to read yourself. After each action its actual result is returned. Use finish only after acceptance or when unable to complete the task within the budget. You have at most twelve actions total.
'''
                if args.no_docs:
                    prompt = prompt.replace('read_language_docs (arguments {"topic":"grammar"}, {"topic":"examples"}, {"topic":"overview"}, or {"topic":"all"}), ', '')
                    prompt = prompt.replace('Choose what to read yourself.', 'No language documentation action exists. Learn only from the available tool schema and feedback.')
                (folder/'initial-prompt.txt').write_text(prompt,encoding='utf-8',newline='\n')
                simple = [shutil.which('node'),str(args.codex_js),'exec','--ignore-user-config','--ephemeral',
                    '--skip-git-repo-check','--json','-s','read-only','-m',MODEL,
                    '-c','model_reasoning_effort="low"','--disable','shell_tool','--disable','multi_agent',
                    '-c','web_search="disabled"','-C',str(work),'-']
                stdout_parts, stderr_parts, usage_parts = [], [], []
                for step in range(12):
                    (folder/f'{step:02d}.prompt.txt').write_text(prompt,encoding='utf-8',newline='\n')
                    turn = subprocess.run(simple,input=prompt,capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=240,**flags)
                    (folder/f'{step:02d}.events.jsonl').write_text(turn.stdout,encoding='utf-8',newline='\n')
                    stdout_parts.append(turn.stdout)
                    stderr_parts.append(turn.stderr)
                    events = [json.loads(line) for line in turn.stdout.splitlines() if line.startswith('{')]
                    usage_events = [e['usage'] for e in events if e['type']=='turn.completed']
                    if turn.returncode or not usage_events:
                        raise RuntimeError(f'Agent completion failed: {turn.stderr[-500:]} {turn.stdout[-500:]}')
                    usage_parts.append(usage_events[-1])
                    message = [e['item']['text'] for e in events if e['type']=='item.completed' and e['item']['type']=='agent_message'][-1]
                    try:
                        action = json.loads(message)
                        tool = action['tool']
                        if tool == 'finish':
                            break
                        if tool == 'discover_tools':
                            answer = {'tools':exposed}
                            calls.append({'tool':'discover_tools','arguments':{},'result':answer})
                            save(folder/'calls.json',calls)
                            print(f'{name}-{arm}: discovered tools',flush=True)
                        else:
                            req = urllib.request.Request(f'http://127.0.0.1:{proxy.server_port}/mcp',
                                data=compact({'jsonrpc':'2.0','id':step,'method':'tools/call','params':{'name':tool,'arguments':action.get('arguments',{})}}).encode(),
                                headers={'Content-Type':'application/json'})
                            with urllib.request.urlopen(req,timeout=40) as response:
                                answer = json.load(response)
                    except (ValueError,KeyError) as error:
                        answer = {'error':'Invalid agent action: '+str(error)}
                    prompt += '\nAGENT ACTION:\n'+message+'\nACTUAL ACTION RESULT:\n'+compact(answer)+'\nChoose the next action.\n'
                save(folder/'per-completion-usage.json',usage_parts)
                combined = {key:sum(u.get(key,0) for u in usage_parts) for key in usage_parts[0]}
                stdout = ''.join(stdout_parts) + compact({'type':'experiment.completed','usage':combined})+'\n'
                run = subprocess.CompletedProcess(simple,0,stdout,'\n'.join(stderr_parts))
        finally:
            proxy.shutdown()
            proxy.server_close()
            thread.join(timeout=5)
        (folder/'events.jsonl').write_text(run.stdout,encoding='utf-8',newline='\n')
        (folder/'stderr.txt').write_text(run.stderr,encoding='utf-8',newline='\n')
        events = [json.loads(line) for line in run.stdout.splitlines() if line.startswith('{')]
        completed = [e['usage'] for e in events if e['type']==('experiment.completed' if args.runner=='harness' else 'turn.completed')]
        if run.returncode or not completed:
            raise RuntimeError(f'Infrastructure failure {name}-{arm}: {run.stderr[-1500:]} {run.stdout[-1500:]}')
        if 'failed to spawn code-mode host' in run.stderr:
            raise RuntimeError(f'Infrastructure failure {name}-{arm}: code-mode host could not start; exclude this trial.')
        usage = completed[-1]
        usage['total_tokens'] = usage['input_tokens'] + usage['output_tokens']
        submissions = [c for c in calls if c['tool']==permitted and not c.get('budget_exceeded')]
        accepted = [bool(c.get('result',{}).get('structuredContent',{}).get('valid')) for c in submissions]
        report = submissions[-1].get('result',{}).get('structuredContent',{}) if submissions else {}
        source = report.get('source','')
        if source:
            (folder/'final.bz').write_text(source,encoding='utf-8',newline='\n')
        items = [e['item'] for e in events if e['type']=='item.completed']
        violations = [i['type'] for i in items if i['type'] in ('command_execution','file_change','web_search')]
        result = {'task':name,'arm':arm,'model':MODEL,'reasoning_effort':'low',
            'first_attempt_accepted':bool(accepted and accepted[0]),
            'compiler_accepted':bool(accepted and accepted[-1]),'submissions':len(submissions),
            'repair_rounds':max(0,len(submissions)-1),
            'doc_calls':sum(c['tool']=='read_language_docs' for c in calls),
            'doc_topics':[c['arguments'].get('topic') for c in calls if c['tool']=='read_language_docs'],
            'usage':usage,'elapsed_seconds':time.monotonic()-started,
            'protocol_violations':violations,'task_correctness':'pending independent source review',
            'runner':args.runner,'discovery_calls':sum(c['tool']=='discover_tools' for c in calls),
            'language_documentation_available':not args.no_docs,
            'final_message':next((i.get('text') for i in reversed(items) if i['type']=='agent_message'),None)}
        save(folder/'result.json',result)
        print(f'FINISHED {name}-{arm}: {usage["total_tokens"]} tokens, {result["repair_rounds"]} repairs, {result["doc_calls"]} doc calls',flush=True)
        return result

    results = []
    try:
        deadline = time.monotonic()+25
        while True:
            try:
                upstream('ping')
                break
            except OSError:
                if time.monotonic()>deadline:
                    raise
                time.sleep(.2)
        upstream('initialize',{'protocolVersion':'2025-11-25','capabilities':{},'clientInfo':{'name':'on-demand-pilot','version':'1'}})
        tools = upstream('tools/list')['result']['tools']
        for index,(name,_,_) in enumerate(TASKS[:args.tasks]):
            with ThreadPoolExecutor(max_workers=2) as pool:
                futures = [pool.submit(trial,name,arm,tools) for arm in (('A','B') if index%2==0 else ('B','A'))]
                for future in futures:
                    results.append(future.result())
            save(out/'results.json',{'model':MODEL,'results':results,
                'method':'Fresh agents choose tools/documentation/submissions through an external text-action loop dispatching real MCP calls; no initial grammar/examples/schemas or conversation history; fresh CLI completion replays trial history on each action; all completions included in usage. Native runner is optional.',
                'runner':args.runner,
                'language_documentation_available':not args.no_docs,
                'max_submissions':4,'runs_per_task_per_arm':1})
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()


if __name__=='__main__':
    main()
