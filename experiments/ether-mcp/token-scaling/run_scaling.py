"""Seven-size representation study.

Requirements: pip install tiktoken==0.14.0 matplotlib
Run: python run_scaling.py --root PATH_TO_BENZENE --output OUTPUT_DIRECTORY
Uses the existing compiled Gleam MCP modules and ether executable; no model API calls.
"""
import argparse
import csv
import json
import os
from pathlib import Path
import shutil
import socket
import subprocess
import time
import urllib.request

import numpy as np
import tiktoken
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator, FuncFormatter

MODEL = 'gpt-6.1-sol'
SIZES = [1, 2, 4, 8, 16, 32, 64]


def named(name):
    return {'kind': 'NamedType', 'name': name}


def identifier(name):
    return {'kind': 'Identifier', 'name': name}


def program(stages):
    """Each added stage contributes one increment function and one main binding."""
    functions = []
    source_functions = []
    bindings = []
    source_bindings = []
    for i in range(1, stages + 1):
        functions.append({
            'kind': 'Function', 'identifier': f'step{i}',
            'param_list': [{'param': 'value', 'data_type': named('Int')}],
            'return_type': named('Int'),
            'body': [{'kind': 'Binary', 'operator': 'Add',
                      'left': identifier('value'), 'right': {'kind': 'Integer', 'value': i}}],
        })
        source_functions.append(f'func step{i}(value: Int) :> Int\n  value + {i}\nend')
        argument = {'kind': 'Integer', 'value': 0} if i == 1 else identifier(f'total{i-1}')
        bindings.append({
            'kind': 'Let', 'identifier': f'total{i}',
            'value': {'kind': 'Call', 'identifier': f'step{i}', 'arguments': [argument]},
        })
        argument_source = '0' if i == 1 else f'total{i-1}'
        source_bindings.append(f'  let total{i} = step{i}({argument_source})')
    functions.append({
        'kind': 'Function', 'identifier': 'main', 'return_type': named('Int'),
        'body': bindings + [identifier(f'total{stages}')],
    })
    source_functions.append('func main() :> Int\n' + '\n'.join(source_bindings)
                            + f'\n  total{stages}\nend')
    return {'construct': {'kind': 'Module', 'data': functions}}, '\n\n'.join(source_functions)


def compact(value):
    return json.dumps(value, separators=(',', ':'), ensure_ascii=False)


def make_plot(rows, fit, output):
    plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 11,
                         'svg.fonttype': 'none', 'axes.titleweight': 'bold'})
    background, ink, green, gold = '#20372e', '#f3efe6', '#82b79a', '#e5b96e'
    x = np.array([r['source_tokens'] for r in rows])
    y = np.array([r['json_tokens'] for r in rows])
    fig, axes = plt.subplots(1, 2, figsize=(12.8, 5.5), facecolor=background)
    for ax in axes:
        ax.set_facecolor(background)
        ax.tick_params(axis='both', which='both', colors=ink)
        ax.xaxis.label.set_color(ink)
        ax.yaxis.label.set_color(ink)
        ax.title.set_color(ink)
        for spine in ax.spines.values():
            spine.set_color('#4e7966')
        ax.grid(color='#4e7966', linewidth=0.7, alpha=0.5)
        ax.set_axisbelow(True)
        ax.set_xlabel('Benzene source tokens')
        ax.set_ylabel('Compact JSON tool-argument tokens')
    fitted_x = np.linspace(0, x.max() * 1.04, 200)
    axes[0].plot(fitted_x, fit['slope'] * fitted_x + fit['intercept'],
                 color=gold, linewidth=1.5, linestyle='--', label='Linear fit')
    axes[0].scatter(x, y, s=58, color=green, edgecolor=ink, linewidth=0.7,
                    zorder=3, label='7 measured program sizes')
    axes[0].set_title('Token counts', loc='left', pad=14, color=ink)
    axes[0].set_xlim(0, x.max() * 1.09)
    axes[0].set_ylim(0, y.max() * 1.10)
    axes[0].xaxis.set_major_locator(MaxNLocator(5))
    axes[0].yaxis.set_major_locator(MaxNLocator(5))
    axes[0].xaxis.set_major_formatter(FuncFormatter(lambda value, _: f'{value:,.0f}'))
    axes[0].yaxis.set_major_formatter(FuncFormatter(lambda value, _: f'{value:,.0f}'))
    legend = axes[0].legend(loc='upper left', frameon=False, fontsize=10)
    for text in legend.get_texts():
        text.set_color(ink)
    axes[1].plot(x, y, color=green, linewidth=1.5, marker='o', markersize=6)
    axes[1].set_xscale('log')
    axes[1].set_yscale('log')
    axes[1].set_title('Orders of magnitude (log scales)', loc='left', pad=14, color=ink)
    for row in rows:
        axes[1].annotate(str(row['stages']), (row['source_tokens'], row['json_tokens']),
                         xytext=(5, 8) if row['stages'] == 1 else (5, -13), textcoords='offset points', color=ink, fontsize=9)
    axes[1].text(0.02, 0.96, 'Labels: number of stages', transform=axes[1].transAxes,
                 color=ink, fontsize=9, va='top')
    fig.suptitle('How does JSON grow with Benzene source?', color=ink,
                 fontsize=20, fontweight='bold', x=0.055, ha='left', y=0.97)
    fig.text(0.055, 0.89, 'Same program family, increasing size | o200k_base text counts',
             color='#b8d0bd', fontsize=11)
    fig.text(0.055, 0.035, 'Repeated increment functions and bindings; compiler-checked. This is not total model usage.',
             color='#b8d0bd', fontsize=10)
    fig.subplots_adjust(left=0.08, right=0.96, top=0.78, bottom=0.18, wspace=0.28)
    fig.savefig(output / 'token-scaling.svg', facecolor=background)
    fig.savefig(output / 'token-scaling.png', facecolor=background, dpi=180)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parent)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    exe = root / ('bin/ether.exe' if os.name == 'nt' else 'bin/ether')
    project = root / 'mcp/ether_mcp'
    libs = project / 'build/dev/erlang'
    erl = shutil.which('erl')
    if not erl or not exe.exists() or not libs.exists():
        raise RuntimeError('Requires Erlang, a built ether compiler, and built Gleam MCP modules')
    encoding = tiktoken.get_encoding('o200k_base')
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    command = [erl, '-noshell', '-pa'] + [str(p) for p in libs.glob('*/ebin')]
    command += ['-eval', "'mcp@server':main()."]
    kwargs = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
    server = subprocess.Popen(command, cwd=project,
        env=dict(os.environ, BENZENE_MCP_PORT=str(port), ETHER_BIN=str(exe)),
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, **kwargs)
    def send(request):
        req = urllib.request.Request(f'http://127.0.0.1:{port}/mcp',
            data=compact(request).encode('utf-8'), headers={
                'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream',
                'MCP-Protocol-Version': '2025-11-25'})
        with urllib.request.urlopen(req, timeout=20) as response:
            body = response.read()
            return json.loads(body) if body else None
    rows = []
    try:
        deadline = time.monotonic() + 20
        while True:
            if server.poll() is not None:
                raise RuntimeError('MCP server exited before listening')
            try:
                send({'jsonrpc': '2.0', 'id': 'ping', 'method': 'ping'})
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise
                time.sleep(0.2)
        send({'jsonrpc': '2.0', 'id': 'initialize', 'method': 'initialize', 'params': {
            'protocolVersion': '2025-11-25', 'capabilities': {},
            'clientInfo': {'name': 'ether-token-scaling', 'version': '1'}}})
        send({'jsonrpc': '2.0', 'method': 'notifications/initialized'})
        tools = send({'jsonrpc': '2.0', 'id': 'list', 'method': 'tools/list'})
        assert 'generate_program' in {t['name'] for t in tools['result']['tools']}
        for stages in SIZES:
            arguments, source = program(stages)
            request = {'jsonrpc': '2.0', 'id': str(stages), 'method': 'tools/call',
                       'params': {'name': 'generate_program', 'arguments': arguments}}
            response = send(request)
            result = response['result']
            report = result['structuredContent']
            assert not result['isError'] and report['validated'] and report['valid'], report
            assert report['source'] == source, 'Renderer differed from the direct-source baseline'
            stem = f'stages-{stages:02d}'
            (out / f'{stem}.bz').write_text(source, encoding='utf-8', newline='\n')
            (out / f'{stem}.arguments.json').write_text(compact(arguments), encoding='utf-8', newline='\n')
            (out / f'{stem}.request.json').write_text(compact(request), encoding='utf-8', newline='\n')
            (out / f'{stem}.validation.json').write_text(json.dumps(response, indent=2), encoding='utf-8')
            checked = subprocess.run([str(exe), 'check', str(out / f'{stem}.bz')],
                                     capture_output=True, text=True, **kwargs)
            assert checked.returncode == 0, checked.stdout + checked.stderr
            source_tokens = len(encoding.encode(source))
            json_tokens = len(encoding.encode(compact(arguments)))
            rows.append({'stages': stages, 'functions': stages + 1,
                         'source_lines': len(source.splitlines()),
                         'source_tokens': source_tokens, 'json_tokens': json_tokens,
                         'json_to_source_ratio': json_tokens / source_tokens,
                         'source_utf8_bytes': len(source.encode('utf-8')),
                         'json_utf8_bytes': len(compact(arguments).encode('utf-8')),
                         'compiler_accepted': True, 'source_matches_mcp_output_exactly': True})
            print(f'{stages} stages: source={source_tokens}, JSON={json_tokens}, ratio={json_tokens/source_tokens:.4f}', flush=True)
    finally:
        server.terminate()
        try:
            server.wait(timeout=5)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait()
    x = np.array([r['source_tokens'] for r in rows], dtype=float)
    y = np.array([r['json_tokens'] for r in rows], dtype=float)
    slope, intercept = np.polyfit(x, y, 1)
    predicted = slope * x + intercept
    residuals = y - predicted
    fit = {'slope': float(slope), 'intercept': float(intercept),
           'r_squared': float(1 - np.sum(residuals**2) / np.sum((y-y.mean())**2)),
           'max_absolute_residual_tokens': float(np.max(np.abs(residuals)))}
    results = {'date': '2026-10-11', 'authoring_model': MODEL,
        'encoding': encoding.name, 'tiktoken_version': tiktoken.__version__,
        'verified_model_encoding_mapping': None,
        'method': 'Deterministic size sweep of one model-authored program family, not seven independent model completions.',
        'text_format': 'Compact tool arguments; UTF-8 LF source; no trailing newline or code fences.',
        'excluded': ['prompts', 'tool schemas', 'reasoning', 'tool responses', 'retries', 'JSON-RPC envelope'],
        'family': 'Each stage adds an Int increment function and a main binding calling it on the previous result.',
        'rows': rows, 'linear_fit': fit}
    (out / 'scaling-results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    with (out / 'scaling-results.csv').open('w', encoding='utf-8', newline='') as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    make_plot(rows, fit, out)
    print(json.dumps({'linear_fit': fit, 'artifacts': str(out)}, indent=2), flush=True)

if __name__ == '__main__':
    main()
