"""Display diagnostic snapshots; only POINT_START controls measurement timing."""
import time

PHASES = {
    "reconfigure": "停止旧配置并释放硬件",
    "gps_acquire": "检测 GPS/PPS 模块并等待定位",
    "hal_init": "初始化芯片/RF 并校准",
    "pps_align": "设置 PPS 周期并等待对齐",
    "slot_config": "配置业务时隙",
    "hal_start": "启动硬件与 PPS 帧监测",
    "ipc_start": "连接 NS/IPC 并请求业务配置",
}


class GatewayProgress:
    def __init__(self, emit=print, clock=time.monotonic):
        self.emit, self.clock = emit, clock
        self.offset = None
        self.began = self.last_print = clock()
        self.stage = "尚未收到网关阶段状态（旧版程序可仅输出频点事件）"
        self.waiting = False
        self.last_snapshot = None

    def event(self, event):
        name = event["event"]
        if name in ("POINT_CONFIG", "POINT_RETRY"):
            self.offset = event["offset_hz"]
            if name == "POINT_CONFIG":
                self.began = self.clock()
            self.last_print = self.clock()
            self.waiting = True
            self.last_snapshot = None
            self.stage = "等待网关上报具体阶段"
        elif name in ("POINT_START", "POINT_END", "POINT_FAILED", "SWEEP_DONE", "SWEEP_FAILED"):
            self.waiting = False

    def status(self, snapshot):
        self.last_snapshot = snapshot
        phase = snapshot.get("phase")
        if phase == "readiness":
            gps = snapshot.get("gps_mode", "unknown")
            received = snapshot.get("ns_received", 0)
            verified = snapshot.get("ns_verified", 0)
            broadcast = snapshot.get("broadcast_valid", 0)
            slots = snapshot.get("s3_count", 0)
            details = (f"GPS={gps}，NS收到={received}/校验={verified}，"
                       f"广播数据有效={broadcast}，S3累计={slots}，"
                       f"S3停滞={snapshot.get('s3_idle_ms', '?')} ms")
            if snapshot.get("ready") == 1:
                reason = "GPS/PPS、NS配置和广播载荷已就绪，等待稳定期/POINT_START"
            elif gps != "EXTERNAL_ACTIVE" or snapshot.get("gps_abnormal", 0):
                reason = "等待 GPS/PPS 就绪"
            elif not received:
                reason = "等待 NS 配置消息；检查 mqtt_bridge/NS 下发链路"
            elif not verified:
                reason = "等待 NS 参数校验"
            elif broadcast != 1:
                reason = "等待有效广播载荷；检查 NS 广播下发"
            elif slots < 2:
                reason = "等待时隙中断推进"
            else:
                reason = "等待网关确认就绪"
            self.stage = details+"；"+reason
        else:
            self.stage = PHASES.get(phase, str(phase))
        if self.waiting:
            self.report()

    def report(self):
        now = self.clock()
        self.emit(f"频差 {self.offset:+d} Hz：准备已用 {now-self.began:.0f} 秒；{self.stage}")
        self.last_print = now

    def tick(self):
        if self.waiting and self.clock()-self.last_print >= 10:
            self.report()

    def diagnostic_line(self, line):
        # Throttled hint for old binaries too. Diagnostic text NEVER gates readiness.
        if "dataLen=0" in line:
            self.stage = "网关报告广播载荷 dataLen=0；等待有效广播载荷，详见 gateway.log"
        elif "NS GPS/TDD/rate/block configuration mismatch" in line:
            self.stage = "NS 下发的 GPS/TDD/速率/包块数与测试参数不一致"
