"""Mechanical reports from designer data and recorded actual-C witnesses."""
import hashlib
import json
from pathlib import Path
from collections import Counter
from verify_campaign_design import FLOORS, ROOT
OUT=ROOT/'artifacts/campaign-production'

DOC=ROOT/'docs/level-design'
ASSETS=ROOT/'assets_src/level_design'
def main():
    DOC.mkdir(parents=True,exist_ok=True)
    lines=['# 五十层逐格设计稿','',
           '2026-10-18。本稿为当前坐标源的机械导出，取代早期仅第一章的地图包。',
           '坐标从0开始。`%`是可直接踏入的暗地砖；其后区域在发现前隐藏。',
           '每层职责说明不等于完整难度验收；路线代价见 CAMPAIGN50_ROUTE_REPORT.md。','']
    for f in FLOORS:
        counts=Counter(''.join(f.rows))
        lines += [f'## {f.number}F · {f.name}', '', f.purpose, '',
                  f'尺寸：{len(f.rows)}×{len(f.rows)}。', '', '```text',*f.rows,'```','']
        for label,chars in [('楼梯/章门','DUG'),('门/暗入口','YBR%'),('钥匙','ybr'),('守卫','svkXmgwM'),('事件/NPC','!n')]:
            found=[f'{c}({x},{y})' for y,row in enumerate(f.rows) for x,c in enumerate(row) if c in chars]
            if found:lines.append(label+'：'+'，'.join(found)+'。')
        lines += ['', '成长/补给：'+', '.join(f'{c}×{counts[c]}' for c in 'adpP' if counts[c])+'。','']
    (DOC/'CAMPAIGN50_MAPS.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    results=json.loads((OUT/'summary.json').read_text(encoding='utf-8'))
    report=['# 五十层真实 C 路线账本','',
            '数据来自当前生产 main/tower.c 和 main/tower_floors.c；50张真实地图，没有后40层fixture。',
            'Python只找相邻格路径，敌人属性、战斗、消费、门钥匙、机关及存档合法性均由C执行。',
            '每次移动、战斗预览、确认、购买及传送均执行C存档导出/新实例导入/再导出字节比较。','',
            '五条见证路线可胜不等于任意操作都可胜，也不是最优策略搜索。',
            '先攻路线终态10HP，余量偏窄；初期多开成长库的路线明显更强。',
            '这些差异应继续试玩评估，不把“全部能过”当成最终平衡已完成。','']
    for result in results:
        trace=json.loads((OUT/(result['name']+'.json')).read_text(encoding='utf-8'))
        report += ['## '+result['name'],'',
                   '|章末|HP|攻|防|黄/蓝/红|金币|累计购攻/购防|',
                   '|---|---:|---:|---:|---|---:|---|']
        for cp in result['checkpoints']:
            if not cp.get('name','').startswith('chapter'):continue
            s=cp['state']
            report.append(f"|{s['floor']+1}F|{s['hp']}|{s['attack']}|{s['defence']}|{s['yellow']}/{s['blue']}/{s['red']}|{s['gold']}|{s['shop_attack']}/{s['shop_defence']}|")
        stats=[Counter() for _ in range(5)]
        for a in trace['actions']:
            before,after=a['before'],a['after'];t=stats[before['floor']//10]
            if a['action'][0]=='confirm' and after['hp']>0:
                t['fights']+=1;t['damage']+=before['damage'];t['gold']+=after['gold']-before['gold']
                t['boss_damage']+=before['damage'] if before['floor']%10==9 else 0
            if a['action'][0]=='move':
                t['healing']+=max(0,after['hp']-before['hp'])
                for key in ('yellow','blue','red'):
                    t[key+'_in']+=max(0,after[key]-before[key]);t[key+'_out']+=max(0,before[key]-after[key])
            if a['action'][0]=='buy':t['spent']+=before['gold']-after['gold']
        report += ['','|发生区域|战斗数|实际扣血|拾取治疗|首领扣血|赚金|花金|黄钥匙产/耗|蓝钥匙产/耗|红钥匙产/耗|',
                   '|---|---:|---:|---:|---:|---:|---:|---|---|---|']
        for i,t in enumerate(stats):
            report.append(f"|{i*10+1}–{(i+1)*10}|{t['fights']}|{t['damage']}|{t['healing']}|{t['boss_damage']}|{t['gold']}|{t['spent']}|{t['yellow_in']}/{t['yellow_out']}|{t['blue_in']}/{t['blue_out']}|{t['red_in']}/{t['red_out']}|")
        report += ['',f"动作数：{result['actions']}。胜利后六锚点1/10/20/30/40/50均实际传送验证。",'']
    (DOC/'CAMPAIGN50_ROUTE_REPORT.md').write_text('\n'.join(report)+'\n',encoding='utf-8')
    files=[p for p in ASSETS.iterdir() if p.is_file() and p.name!='campaign_manifest.json' and (p.name.startswith('campaign_') or p.name.startswith('region') and p.suffix=='.py')]
    manifest=dict(revision='tower50-authored-v8',status='production model and Host verified; device QA pending',
                  files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)},
                  model=json.loads((OUT/'model-provenance.json').read_text(encoding='utf-8')),
                  routes=[dict(name=r['name'],final=r['final'],actions=r['actions']) for r in results])
    (ASSETS/'campaign_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('WROTE: 50-floor maps, actual-C route ledger and SHA256 manifest')
if __name__=='__main__':main()
