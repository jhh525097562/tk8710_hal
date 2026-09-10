importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);

var script = ScriptingEnvironment.instance();
script.setScriptTimeout(30000);
script.traceSetConsoleLevel(TraceLevel.INFO);
var server = script.getServer("DebugServer.1");
server.setConfig("D:/ti_workspace/tms570ls3137_halcogen_base_570RAM_add/targetConfigs/TMS570LS3137.ccxml");
var session = server.openSession(".*CortexR4.*");
session.target.connect();
session.target.reset();
session.target.runAsynch();
session.target.disconnect();
session.terminate();
server.stop();
