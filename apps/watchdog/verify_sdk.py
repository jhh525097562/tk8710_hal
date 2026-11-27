"""Read-only checks for the final SDK watchdog integration."""
import hashlib,json,pathlib,re,subprocess
sdk=pathlib.Path('/home/hjh/work/rk3506/rk3506_linux6.1_sdk_v1.2.0')
apps=sdk/'external/rk3506app';kernel=sdk/'kernel-6.1'
overlay=sdk/'buildroot/board/vanxoak/hd_rk3506_iot_emmc/fs-overlay'
cfg=(kernel/'.config').read_text()
for key in ['WATCHDOG','DW_WATCHDOG','WATCHDOG_NOWAYOUT','WATCHDOG_SYSFS']:
    assert re.search(r'^CONFIG_'+key+'=y$',cfg,re.M),key
dtb=kernel/'arch/arm/boot/dts/vanxoak-hd-rk3506b-iot-emmc-v1.dtb'
tree=subprocess.check_output([str(kernel/'scripts/dtc/dtc'),'-I','dtb','-O','dts',str(dtb)],stderr=subprocess.DEVNULL,text=True)
wdt=re.search(r'watchdog@ff260000\s*\{.*?\n\s*\};',tree,re.S)
assert wdt and 'status = "okay"' in wdt.group() and 'timeout-sec = <0x3c>' in wdt.group()
other=re.search(r'watchdog@ff268000\s*\{.*?\n\s*\};',tree,re.S)
assert not other or 'status = "disabled"' in other.group()
for src,dst in [(apps/'build_sdk/bin/rk_wdt',overlay/'usr/sbin/rk_wdt'),
                (apps/'watchdog/S15rk-watchdog',overlay/'etc/init.d/S15rk-watchdog')]:
    assert src.read_bytes()==dst.read_bytes(),dst
assert 'rk_wdt' not in (overlay/'etc/tdaemon.conf').read_text(errors='replace')
assert '# CONFIG_WDT is not set' in (sdk/'u-boot/.config').read_text()
br=(sdk/'buildroot/output/rockchip_hd_rk3506b_iot_emmc/.config').read_text()
assert '# BR2_PACKAGE_BUSYBOX_WATCHDOG is not set' in br
assert not (overlay/'etc/init.d/S15watchdog').exists()
paths=[apps/'build_sdk/bin/rk_wdt',apps/'build_sdk/bin/web_server',kernel/'zboot.img',dtb]
report={'wdt0_enabled':True,'wdt1_disabled_or_omitted':True,'nowayout':True,'sysfs':True,
        'independent_of_tdaemon':True,'busybox_autostart_disabled':True,
        'bootloader_wdt_disabled':True,'board_flash_performed':False,
        'artifacts':{str(p.relative_to(sdk)):{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths}}
print(json.dumps(report,indent=2))
