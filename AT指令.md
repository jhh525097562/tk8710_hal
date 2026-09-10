模式6：
AT+SETPARAM=6,0,0,0,0,52,0,52,0,477800000,126,46,255,255
AT+SETMODE=3
AT+STATE
AT+TM
AT+STOP

模式7：
AT+SETPARAM=6,0,0,0,0,52,0,52,0,509100000,126,46,255,255
AT+SETMODE=3
AT+STATE
AT+TM
AT+STOP

模式8：
AT+SETPARAM=8,0,0,0,0,52,0,52,0,477800000,126,46,255,255
AT+SETMODE=3
AT+STATE
AT+TM
AT+STOP

发送Tone：
AT+SETPARAM=8,0,0,0,0,52,0,52,0,477800000,126,46,255,255
AT+SETMODE=4
AT+STOP

ACM校准：
AT+SETPARAM=6,0,0,0,0,52,0,52,0,477800000,126,46,255,255
AT+SETMODE=5

扫频：
AT+SETPARAM=6,0,0,0,0,52,0,52,0,477800000,126,46,255,255
AT+SETMODE=0

采数：
AT+SETPARAM=6,0,0,0,0,52,0,52,0,477800000,126,46,255,255
AT+SETMODE=6


# 仅增量编译
.\ccs_build_flash.ps1 build

# 清理后完整编译
.\ccs_build_flash.ps1 build -Clean

# 编译、烧录、校验，随后运行
.\ccs_build_flash.ps1 build-flash

# 编译和烧录，但烧录后不自动运行
.\ccs_build_flash.ps1 build-flash -NoRun


powershell -ExecutionPolicy Bypass -File .\scripts\build-bootloader.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\build-tms570.ps1 -Configuration Debug -FpgaSelfTest -DataTransferSpi2SlaveTest
powershell -ExecutionPolicy Bypass -File .\scripts\build-combined-image.ps1
F:\ti\uniflash_9.6.0\dslite.bat --config=targetConfigs\TMS570LS3137.ccxml --flash combined\tms570_boot_app_flash.hex --verify --verbose


.\scripts\flash-tms570.ps1 -Image Combined -FpgaSelfTest -DataTransferSpi2SlaveTest

.\scripts\flash-tms570.ps1 -Image Combined -DataTransferSpi2SlaveTest
