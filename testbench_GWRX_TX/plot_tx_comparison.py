"""Backfill shared TX figures for an existing comparison result."""
import argparse
import json
from pathlib import Path
import sys
from tx_comparison_plots import generate_tx_plots


def main():
    sys.stdout.reconfigure(errors='backslashreplace')
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--variant',help='before_wrap / after_wrap for multi-variant results')
    args=parser.parse_args();run=args.run.resolve()
    source=run/'comparison.json'
    if not source.exists():source=run/'result.json'
    data=json.loads(source.read_text(encoding='utf-8'))
    if args.variant:
        if args.variant not in data.get('runs',{}):raise ValueError('Unknown variant')
        reference=Path(data['source_matlab_data'])
        actual=run/args.variant/'GWTXModule/case95/tx_frame0.txt';output=run/args.variant
    else:
        if 'runs' in data:raise ValueError('Choose --variant for multi-variant results')
        reference=Path(data.get('reused_matlab_data',data.get('source_matlab_data',str(run/'matlab'))))
        if not reference.exists() and 'run_command' in data:
            command=data['run_command'];reference=Path(command[command.index('--input-root')+1])
        actual=run/'cmodel/GWTXModule/case95/tx_frame0.txt';output=run
    meta=data.get('matlab_metadata',{})
    context='历史结果补图'
    if 'default_config' in data:context=f'SVD最多{data["default_config"]["max_sweeps"]}轮，同源MATLAB TX输入'
    plots=generate_tx_plots(reference/'matlab_tx_8ant_baseband.txt',actual,output,
        mode=data.get('numeric_mode','unspecified'),scale=meta.get('cmodel_tx_scale',data.get('scale',2**23)),
        sample_rate=meta.get('tx_baseband_rate',125000),context=context)
    (Path(plots['directory'])/'source_result.json').write_text(
        json.dumps({'source_result':str(source),'plots':plots},ensure_ascii=False,indent=2),encoding='utf-8')


if __name__=='__main__':main()
