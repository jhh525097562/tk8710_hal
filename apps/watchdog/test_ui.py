"""Browser regression using actual status-page JS and isolated API fixtures."""
import json, pathlib, re
from playwright.sync_api import sync_playwright
here=pathlib.Path(__file__).resolve().parent
web=here.parent/'webserver'
out=here/'validation-20260922';out.mkdir(exist_ok=True)
fragment=(web/'web_resources/html/status_resources.html').read_text(encoding='utf8')
fragment=re.sub(r'%llu|%d','0',fragment).replace('%%','%')
css=(web/'web_resources/css/style.css').read_text(encoding='utf8')
html='<html><head><meta charset="utf-8"><style>'+css+'</style></head><body><div style="padding:20px;max-width:700px"><div class="status-card status-card--system"><div class="status-card-header">系统信息</div><div class="status-card-body"></div></div>'+fragment+'</div></body></html>'
base={'hostname':'test','model':'RK3506','arch':'ARMv7','device_coords':'--','firmware_version':'test',
      'kernel_version':'6.1','local_time':'test','uptime':'test','load_avg':[0,0,0],'app_version':'test',
      'disk_percent':0,'disk_used':0,'disk_total':1,'mem_percent':0,'mem_used':0,'mem_total':1,
      'tk8710_gw':{'running':True,'uptime_s':100,'version':'test'},'gps_enabled':True,'rfstatus':'11111111'}
cases={'waiting':'等待设备就绪','failed':'启动失败','feeding':'喂狗正常','degraded':'已喂狗',
       'stale':'喂狗状态异常','fault':'喂狗故障','stopped':'喂狗已停止','unavailable':'未启动或未部署'}
results=[]
with sync_playwright() as pw:
    browser=pw.chromium.launch(channel='chrome',headless=True)
    page=browser.new_page(viewport={'width':1050,'height':950})
    errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
    current=dict(base)
    def route(req):
        if req.request.url.endswith('/api/system_info'):
            req.fulfill(status=200,content_type='application/json',body=json.dumps(current))
        else:req.fulfill(status=200,content_type='text/html',body=html)
    page.route('http://wdt.test/**',route)
    for state,label in cases.items():
        current['watchdog']={'state':state,'reason':'device_missing','armed':True,'actual_timeout_s':89,'feed_interval_s':5}
        page.goto('http://wdt.test/status')
        page.wait_for_function('(label)=>document.getElementById("sys-watchdog").textContent.startsWith(label)',arg=label)
        assert page.locator('#gw-running').inner_text()=='运行中'
        assert page.locator('#gw-gps').inner_text()=='已开启'
        results.append({'state':state,'text':page.locator('#sys-watchdog').inner_text()})
        if state in ['failed','feeding']:page.screenshot(path=str(out/('web-'+state+'.png')),full_page=True)
    assert not errors,errors
    browser.close()
(out/'web-ui.json').write_text(json.dumps({'cases':results,'javascript_errors':errors,'fixture_only':True},ensure_ascii=False,indent=2),encoding='utf8')
print('PASS eight watchdog UI states; existing gateway/GPS rows preserved; no JS errors')
