$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$DefaultServerAddr = 'amqtt.taolink-tech.com'
$DefaultServerPort = 8866
$DefaultWebPort = 18090
$DefaultShellPort = 10022
$TextAddGateway = -join @([char]0x6DFB, [char]0x52A0, [char]0x7F51, [char]0x5173)
$TextDeleteGateway = -join @([char]0x5220, [char]0x9664, [char]0x7F51, [char]0x5173)
$TextStart = -join @([char]0x542F, [char]0x52A8)
$TextStop = -join @([char]0x505C, [char]0x7528)
$TextOpenWeb = -join @([char]0x6253, [char]0x5F00, [char]0x9875, [char]0x9762)

$configPath = Join-Path $scriptDir 'devices.json'
$frpcPath = Join-Path $scriptDir 'frpc.exe'
$script:gatewayRows = New-Object System.Collections.ArrayList

function Normalize-RK3506DeviceId {
    param(
        [Parameter(Mandatory = $true)]
        [string]$DeviceId
    )

    $normalized = $DeviceId.Trim()
    if ($normalized -notmatch '^[A-Za-z0-9_-]{16}$') {
        throw "Gateway ID must be exactly 16 characters and may only contain letters, numbers, underscore, or hyphen. Input: $DeviceId"
    }

    return $normalized
}

function Escape-RK3506TomlString {
    param([string]$Value)
    return $Value.Replace('`', '``').Replace('"', '\"')
}

function Test-LocalPortAvailable {
    param([int]$Port)

    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
    try {
        $listener.Start()
        return $true
    }
    catch [System.Net.Sockets.SocketException] {
        return $false
    }
    finally {
        $listener.Stop()
    }
}

function Test-RK3506PortReservedByOtherRow {
    param([int]$Port, [int]$Index)

    for ($i = 0; $i -lt $script:gatewayRows.Count; $i++) {
        if ($i -eq $Index) { continue }
        $other = $script:gatewayRows[$i]
        if ($other.Started -and ($other.WebPort -eq $Port -or $other.ShellPort -eq $Port)) { return $true }
        if ($other.PortGatewayId -and $other.GatewayTextBox -and
            $other.PortGatewayId -eq $other.GatewayTextBox.Text.Trim() -and
            ($other.SavedWebPort -eq $Port -or $other.SavedShellPort -eq $Port)) { return $true }
    }
    return $false
}

function Get-WebPortForRow {
    param([int]$Index)
    $port = $DefaultWebPort + $Index
    while ($port -le 65535 -and (
        (Test-RK3506PortReservedByOtherRow -Port $port -Index $Index) -or
        -not (Test-LocalPortAvailable $port))) {
        $port++
    }
    if ($port -gt 65535) { throw 'No free local Web port is available.' }
    return $port
}

function Get-ShellPortForRow {
    param([int]$Index, [int]$WebPort = 0)
    $port = $DefaultShellPort + $Index
    while ($port -le 65535 -and (
        $port -eq $WebPort -or
        (Test-RK3506PortReservedByOtherRow -Port $port -Index $Index) -or
        -not (Test-LocalPortAvailable $port))) {
        $port++
    }
    if ($port -gt 65535) { throw 'No free local SSH port is available.' }
    return $port
}

function Wait-VisitorPorts {
    param($Process, [int]$WebPort, [int]$ShellPort, [int]$TimeoutSeconds = 15)

    if ($WebPort -eq $ShellPort) { throw 'Web and SSH visitors cannot share a local port.' }

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if ($Process.HasExited) { throw "frpc.exe exited before opening local ports (status=$($Process.ExitCode))." }

        $ownedPorts = @()
        foreach ($line in @(netstat.exe -ano -p tcp)) {
            if ($line -match '^\s*TCP\s+127\.0\.0\.1:(\d+)\s+\S+\s+LISTENING\s+(\d+)\s*$' -and
                [int]$Matches[2] -eq $Process.Id) {
                $ownedPorts += [int]$Matches[1]
            }
        }
        if ($ownedPorts -contains $WebPort -and $ownedPorts -contains $ShellPort) { return }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)

    throw "frpc.exe did not open both local ports ($WebPort, $ShellPort). Check the log for bind or connection errors."
}

function New-RK3506VisitorConfig {
    param(
        [Parameter(Mandatory = $true)]
        [string]$OutputPath,

        [Parameter(Mandatory = $true)]
        [string]$DeviceId,

        [int]$WebBindPort,

        [int]$SshBindPort
    )

    $deviceIdValue = Normalize-RK3506DeviceId $DeviceId
    $serverAddrValue = Escape-RK3506TomlString $DefaultServerAddr
    $token = '9763d8a8ff41a4d0648edcfdc510bc0fd0072954459025d9f6fd208acb214fab'
    $content = @"
serverAddr = "$serverAddrValue"
serverPort = $DefaultServerPort

auth.method = "token"
auth.token = "$token"

[[visitors]]
name = "rk3506-$deviceIdValue-web-visitor"
type = "stcp"
serverName = "rk3506-$deviceIdValue-web"
secretKey = "$token"
bindAddr = "127.0.0.1"
bindPort = $WebBindPort

[[visitors]]
name = "rk3506-$deviceIdValue-ssh-visitor"
type = "stcp"
serverName = "rk3506-$deviceIdValue-ssh"
secretKey = "$token"
bindAddr = "127.0.0.1"
bindPort = $SshBindPort
"@

    Set-Content -LiteralPath $OutputPath -Value $content -Encoding ASCII
    return $OutputPath
}

function New-RK3506LauncherDefaultConfig {
    return [pscustomobject]@{
        serverAddr = $DefaultServerAddr
        serverPort = $DefaultServerPort
        webBindPort = $DefaultWebPort
        sshBindPort = $DefaultShellPort
        gateways = @()
    }
}

function New-RK3506PortPair {
    param($WebPort, $ShellPort, [string]$DeviceId)

    $web = 0
    $shell = 0
    if (-not [int]::TryParse([string]$WebPort, [ref]$web) -or
        -not [int]::TryParse([string]$ShellPort, [ref]$shell) -or
        $web -lt 1 -or $web -gt 65535 -or $shell -lt 1 -or $shell -gt 65535 -or
        $web -eq $shell) {
        throw "Invalid saved Web/SSH port pair for gateway $DeviceId."
    }
    return [pscustomobject]@{ WebPort = $web; ShellPort = $shell }
}

function Normalize-RK3506LauncherConfig {
    param($Config)

    if (-not $Config) { return New-RK3506LauncherDefaultConfig }
    if (-not $Config.PSObject.Properties['gateways']) {
        $gatewayRows = @()
        if ($Config.PSObject.Properties['devices'] -and $Config.devices) {
            foreach ($device in @($Config.devices)) {
                if ($device.PSObject.Properties['gatewayId'] -and -not [string]::IsNullOrWhiteSpace($device.gatewayId)) {
                    $gatewayRows += [pscustomobject]@{ gatewayId = [string]$device.gatewayId }
                }
            }
        }
        $Config | Add-Member -Force -NotePropertyName gateways -NotePropertyValue $gatewayRows
    }
    $Config.serverAddr = $DefaultServerAddr
    $Config.serverPort = $DefaultServerPort
    $Config.webBindPort = $DefaultWebPort
    $Config.sshBindPort = $DefaultShellPort
    return $Config
}

function Load-RK3506LauncherConfigFile {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return New-RK3506LauncherDefaultConfig
    }

    try {
        return Normalize-RK3506LauncherConfig (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json)
    }
    catch {
        $backupPath = "$Path.corrupt.$(Get-Date -Format 'yyyyMMddHHmmss')"
        Move-Item -LiteralPath $Path -Destination $backupPath -Force
        return New-RK3506LauncherDefaultConfig
    }
}

function Save-RK3506LauncherConfigFile {
    param([string]$Path)

    $gateways = @()
    foreach ($row in @($script:gatewayRows)) {
        $gatewayId = $row.GatewayTextBox.Text.Trim()
        if (-not [string]::IsNullOrWhiteSpace($gatewayId)) {
            $entry = [ordered]@{ gatewayId = $gatewayId }
            if ($row.PortGatewayId -eq $gatewayId -and
                ($null -ne $row.SavedWebPort -or $null -ne $row.SavedShellPort)) {
                $ports = New-RK3506PortPair -WebPort $row.SavedWebPort -ShellPort $row.SavedShellPort -DeviceId $gatewayId
                $entry.webBindPort = $ports.WebPort
                $entry.sshBindPort = $ports.ShellPort
            }
            $gateways += [pscustomobject]$entry
        }
    }

    $config = [pscustomobject]@{
        serverAddr = $DefaultServerAddr
        serverPort = $DefaultServerPort
        webBindPort = $DefaultWebPort
        sshBindPort = $DefaultShellPort
        gateways = $gateways
    }

    $tmpPath = "$Path.tmp.$([guid]::NewGuid().ToString('N'))"
    $config | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $tmpPath -Encoding ASCII
    Move-Item -LiteralPath $tmpPath -Destination $Path -Force
}

function Load-RK3506LauncherConfig {
    return Load-RK3506LauncherConfigFile -Path $configPath
}

function Save-RK3506LauncherConfig {
    Save-RK3506LauncherConfigFile -Path $configPath
}

function New-Label {
    param([string]$Text, [int]$X, [int]$Y, [int]$Width = 100)
    $label = New-Object System.Windows.Forms.Label
    $label.Text = $Text
    $label.Location = New-Object System.Drawing.Point($X, $Y)
    $label.Size = New-Object System.Drawing.Size($Width, 22)
    return $label
}

function New-TextBox {
    param([int]$X, [int]$Y, [int]$Width = 180)
    $textBox = New-Object System.Windows.Forms.TextBox
    $textBox.Location = New-Object System.Drawing.Point($X, $Y)
    $textBox.Size = New-Object System.Drawing.Size($Width, 22)
    return $textBox
}

function New-LinkTextBox {
    param([int]$X, [int]$Y, [int]$Width = 120)
    $textBox = New-TextBox $X $Y $Width
    $textBox.ReadOnly = $true
    $textBox.BackColor = [System.Drawing.SystemColors]::Control
    $textBox.ForeColor = [System.Drawing.SystemColors]::GrayText
    return $textBox
}

function New-Button {
    param([string]$Text, [int]$X, [int]$Y, [int]$Width = 90)
    $button = New-Object System.Windows.Forms.Button
    $button.Text = $Text
    $button.Location = New-Object System.Drawing.Point($X, $Y)
    $button.Size = New-Object System.Drawing.Size($Width, 30)
    return $button
}

function Show-ErrorMessage {
    param([string]$Message)
    [System.Windows.Forms.MessageBox]::Show($Message, 'RK3506 Tunnel Launcher', [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error) | Out-Null
}

function Append-Log {
    param([string]$Message)
    if ([string]::IsNullOrWhiteSpace($Message)) { return }
    $action = {
        param($Text)
        $logTextBox.AppendText("[$(Get-Date -Format 'HH:mm:ss')] $Text`r`n")
    }
    if ($logTextBox.InvokeRequired) {
        $logTextBox.BeginInvoke($action, @($Message)) | Out-Null
    } else {
        & $action $Message
    }
}

function Get-GatewayRowIndex {
    param($Row)
    return $script:gatewayRows.IndexOf($Row)
}

function Get-VisitorConfigPath {
    param([string]$DeviceId)
    return Join-Path $scriptDir "frpc-visitor-$DeviceId.toml"
}

function Get-RK3506LegacyVisitorPorts {
    param([string]$DeviceId)

    $DeviceId = Normalize-RK3506DeviceId $DeviceId
    $path = Get-VisitorConfigPath $DeviceId
    if (-not (Test-Path -LiteralPath $path)) { return $null }

    $visitorName = ''
    $webPort = $null
    $shellPort = $null
    foreach ($line in @(Get-Content -LiteralPath $path)) {
        if ($line -match '^\s*\[\[visitors\]\]\s*$') {
            $visitorName = ''
        }
        elseif ($line -match '^\s*name\s*=\s*"([^"]+)"\s*$') {
            $visitorName = $Matches[1]
        }
        elseif ($line -match '^\s*bindPort\s*=\s*(\d+)\s*$') {
            if ($visitorName -eq "rk3506-$DeviceId-web-visitor") { $webPort = $Matches[1] }
            if ($visitorName -eq "rk3506-$DeviceId-ssh-visitor") { $shellPort = $Matches[1] }
        }
    }
    if ($null -eq $webPort -or $null -eq $shellPort) {
        throw "Could not recover the last ports for gateway $DeviceId from $path."
    }
    return New-RK3506PortPair -WebPort $webPort -ShellPort $shellPort -DeviceId $DeviceId
}

function Get-RK3506PreferredPorts {
    param($Row, [string]$DeviceId)

    if ($Row.PortGatewayId -eq $DeviceId -and
        ($null -ne $Row.SavedWebPort -or $null -ne $Row.SavedShellPort)) {
        return New-RK3506PortPair -WebPort $Row.SavedWebPort -ShellPort $Row.SavedShellPort -DeviceId $DeviceId
    }
    return Get-RK3506LegacyVisitorPorts -DeviceId $DeviceId
}

function Remove-GatewayVisitorConfig {
    param($Row)

    $paths = @()
    if ($Row.VisitorConfigPath) {
        $paths += [string]$Row.VisitorConfigPath
    }
    try {
        $deviceId = Normalize-RK3506DeviceId $Row.GatewayTextBox.Text
        $paths += Get-VisitorConfigPath $deviceId
    }
    catch { }

    foreach ($path in @($paths | Select-Object -Unique)) {
        if ($path -and (Test-Path -LiteralPath $path)) {
            Remove-Item -LiteralPath $path -Force
        }
    }
}

function Set-GatewayRowStarted {
    param($Row, [bool]$Started)
    $Row.Started = $Started
    $Row.GatewayTextBox.Enabled = -not $Started
    $Row.OpenButton.Enabled = $Started
    if ($Started) {
        $Row.ToggleButton.Text = $TextStop
    }
    else {
        $Row.ToggleButton.Text = $TextStart
        $Row.WebPortTextBox.Text = ''
        $Row.ShellPortTextBox.Text = ''
        $Row.WebPort = $null
        $Row.ShellPort = $null
    }
}

function Start-GatewayRow {
    param($Row)

    if (-not (Test-Path -LiteralPath $frpcPath)) { throw "Missing frpc.exe: $frpcPath" }
    if ($Row.Started) { return }

    $deviceId = Normalize-RK3506DeviceId $Row.GatewayTextBox.Text
    if ($Row.PortGatewayId -eq $deviceId -and $Row.PortLoadError) { throw $Row.PortLoadError }
    $index = Get-GatewayRowIndex $Row
    if ($index -lt 0) { throw 'Gateway row was not found.' }
    if (@($script:gatewayRows | Where-Object {
        $_.Started -and $_.GatewayTextBox.Text.Trim() -eq $deviceId
    }).Count -gt 0) { throw "Gateway $deviceId is already started in another row." }

    $preferredPorts = Get-RK3506PreferredPorts -Row $Row -DeviceId $deviceId
    if ($preferredPorts) {
        $webPort = $preferredPorts.WebPort
        $shellPort = $preferredPorts.ShellPort
        foreach ($port in @($webPort, $shellPort)) {
            if ((Test-RK3506PortReservedByOtherRow -Port $port -Index $index) -or
                -not (Test-LocalPortAvailable $port)) {
                throw "Saved local port $port for gateway $deviceId is occupied. Close its owner before starting this gateway; the Xshell port was not changed."
            }
        }
    }
    else {
        $webPort = Get-WebPortForRow $index
        $shellPort = Get-ShellPortForRow -Index $index -WebPort $webPort
    }
    $visitorConfigPath = Get-VisitorConfigPath $deviceId
    $visitorConfigExisted = Test-Path -LiteralPath $visitorConfigPath
    New-RK3506VisitorConfig -OutputPath $visitorConfigPath -DeviceId $deviceId -WebBindPort $webPort -SshBindPort $shellPort | Out-Null

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $frpcPath
    $startInfo.Arguments = "-c `"$visitorConfigPath`""
    $startInfo.WorkingDirectory = $scriptDir
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    $eventPrefix = "rk3506-$([guid]::NewGuid().ToString('N'))"
    $outputEventId = "$eventPrefix-stdout"
    $errorEventId = "$eventPrefix-stderr"
    $outputJob = $null
    $errorJob = $null
    $processStarted = $false
    try {
        $outputJob = Register-ObjectEvent -InputObject $process -EventName OutputDataReceived -SourceIdentifier $outputEventId -Action { if ($EventArgs.Data) { Append-Log $EventArgs.Data } }
        $errorJob = Register-ObjectEvent -InputObject $process -EventName ErrorDataReceived -SourceIdentifier $errorEventId -Action { if ($EventArgs.Data) { Append-Log $EventArgs.Data } }

        if (-not $process.Start()) { throw 'Failed to start frpc.exe.' }
        $processStarted = $true
        $process.BeginOutputReadLine()
        $process.BeginErrorReadLine()
        Wait-VisitorPorts -Process $process -WebPort $webPort -ShellPort $shellPort
    }
    catch {
        if ($processStarted) {
            try {
                if (-not $process.HasExited) { $process.Kill() }
                $process.WaitForExit(3000) | Out-Null
            }
            catch [System.InvalidOperationException] { }
        }
        Unregister-Event -SourceIdentifier $outputEventId -ErrorAction SilentlyContinue
        Unregister-Event -SourceIdentifier $errorEventId -ErrorAction SilentlyContinue
        if ($outputJob) { Remove-Job -Job $outputJob -Force -ErrorAction SilentlyContinue }
        if ($errorJob) { Remove-Job -Job $errorJob -Force -ErrorAction SilentlyContinue }
        $process.Dispose()
        if (-not $visitorConfigExisted) { Remove-Item -LiteralPath $visitorConfigPath -Force -ErrorAction SilentlyContinue }
        throw
    }

    $Row.Process = $process
    $Row.OutputEventId = $outputEventId
    $Row.ErrorEventId = $errorEventId
    $Row.OutputJob = $outputJob
    $Row.ErrorJob = $errorJob
    $Row.VisitorConfigPath = $visitorConfigPath
    $Row.WebPort = $webPort
    $Row.ShellPort = $shellPort
    $Row.PortGatewayId = $deviceId
    $Row.SavedWebPort = $webPort
    $Row.SavedShellPort = $shellPort
    $Row.PortLoadError = $null
    $Row.WebPortTextBox.Text = "http://127.0.0.1:$webPort/login"
    $Row.ShellPortTextBox.Text = "127.0.0.1:$shellPort"
    Set-GatewayRowStarted $Row $true
    Save-RK3506LauncherConfig
    Append-Log "Started gateway=$deviceId web=http://127.0.0.1:$webPort/login shell=127.0.0.1:$shellPort"
}

function Stop-GatewayRow {
    param($Row)

    if ($Row.Process) {
        try {
            if (-not $Row.Process.HasExited) { $Row.Process.Kill() }
            if (-not $Row.Process.WaitForExit(3000)) {
                throw "frpc.exe for gateway $($Row.GatewayTextBox.Text.Trim()) did not exit."
            }
        }
        catch [System.InvalidOperationException] { }
    }
    if ($Row.OutputEventId) { Unregister-Event -SourceIdentifier $Row.OutputEventId }
    if ($Row.ErrorEventId) { Unregister-Event -SourceIdentifier $Row.ErrorEventId }
    if ($Row.OutputJob) { Remove-Job -Job $Row.OutputJob -Force }
    if ($Row.ErrorJob) { Remove-Job -Job $Row.ErrorJob -Force }
    if ($Row.Process) { $Row.Process.Dispose() }
    $Row.Process = $null
    $Row.OutputEventId = $null
    $Row.ErrorEventId = $null
    $Row.OutputJob = $null
    $Row.ErrorJob = $null
    Set-GatewayRowStarted $Row $false
    Append-Log "Stopped gateway=$($Row.GatewayTextBox.Text.Trim())"
}

function Open-GatewayWeb {
    param($Row)

    if (-not $Row.Started -or -not $Row.WebPort) { return }
    Start-Process "http://127.0.0.1:$($Row.WebPort)/login"
}

function Get-RK3506LauncherLayoutMetrics {
    $contentWidth = [Math]::Max(950, $form.ClientSize.Width - 40)
    $webLinkWidth = [Math]::Max(300, $contentWidth - 650)
    $shellX = 195 + $webLinkWidth + 10
    $actionX = $shellX + 150 + 15

    return [pscustomobject]@{
        ContentWidth = $contentWidth
        GatewayIdWidth = 180
        WebLinkWidth = $webLinkWidth
        ShellX = $shellX
        ShellWidth = 150
        ActionX = $actionX
        ToggleX = $actionX
        OpenX = $actionX + 80
        DeleteX = $actionX + 180
    }
}

function Update-RK3506LauncherLayout {
    if (-not $form -or -not $gatewayPanel -or -not $logTextBox) { return }

    $layout = Get-RK3506LauncherLayoutMetrics
    $contentWidth = $layout.ContentWidth
    $webLinkWidth = $layout.WebLinkWidth
    $gatewayPanelHeight = [Math]::Max(210, [int](($form.ClientSize.Height - 150) * 0.45))
    $logLabelY = 84 + $gatewayPanelHeight + 10
    $logY = $logLabelY + 24
    $logHeight = [Math]::Max(120, $form.ClientSize.Height - $logY - 20)

    $gatewayHeaderPanel.Size = New-Object System.Drawing.Size($contentWidth, 24)
    $gatewayPanel.Size = New-Object System.Drawing.Size($contentWidth, $gatewayPanelHeight)
    $logLabel.Location = New-Object System.Drawing.Point(20, $logLabelY)
    $logTextBox.Location = New-Object System.Drawing.Point(20, $logY)
    $logTextBox.Size = New-Object System.Drawing.Size($contentWidth, $logHeight)

    $gatewayIdHeader.Size = New-Object System.Drawing.Size($layout.GatewayIdWidth, 22)
    $webLinkHeader.Size = New-Object System.Drawing.Size($layout.WebLinkWidth, 22)
    $shellLinkHeader.Location = New-Object System.Drawing.Point($layout.ShellX, 2)
    $shellLinkHeader.Size = New-Object System.Drawing.Size($layout.ShellWidth, 22)
    $actionHeader.Location = New-Object System.Drawing.Point($layout.ActionX, 2)

    foreach ($row in @($script:gatewayRows)) {
        $row.Panel.Size = New-Object System.Drawing.Size($contentWidth, 36)
        $row.GatewayTextBox.Size = New-Object System.Drawing.Size($layout.GatewayIdWidth, 22)
        $row.WebPortTextBox.Size = New-Object System.Drawing.Size($webLinkWidth, 22)
        $row.ShellPortTextBox.Location = New-Object System.Drawing.Point($layout.ShellX, 7)
        $row.ShellPortTextBox.Size = New-Object System.Drawing.Size($layout.ShellWidth, 22)
        $row.ToggleButton.Location = New-Object System.Drawing.Point($layout.ToggleX, 3)
        $row.OpenButton.Location = New-Object System.Drawing.Point($layout.OpenX, 3)
        $row.DeleteButton.Location = New-Object System.Drawing.Point($layout.DeleteX, 3)
    }
}

function Add-GatewayRow {
    param([string]$GatewayId = '', $SavedWebPort = $null, $SavedShellPort = $null)

    $portLoadError = $null
    if ($GatewayId -and $null -eq $SavedWebPort -and $null -eq $SavedShellPort) {
        try {
            $legacyPorts = Get-RK3506LegacyVisitorPorts -DeviceId $GatewayId
            if ($legacyPorts) {
                $SavedWebPort = $legacyPorts.WebPort
                $SavedShellPort = $legacyPorts.ShellPort
            }
        }
        catch { $portLoadError = $_.Exception.Message }
    }

    $layout = Get-RK3506LauncherLayoutMetrics
    $webLinkWidth = $layout.WebLinkWidth

    $panel = New-Object System.Windows.Forms.Panel
    $panel.Size = New-Object System.Drawing.Size($layout.ContentWidth, 36)

    $gatewayTextBox = New-TextBox 0 7 180
    $gatewayTextBox.Text = $GatewayId
    $gatewayTextBox.MaxLength = 16
    $webPortTextBox = New-LinkTextBox 195 7 $webLinkWidth
    $shellPortTextBox = New-LinkTextBox $layout.ShellX 7 $layout.ShellWidth
    $toggleButton = New-Button $TextStart $layout.ToggleX 3 70
    $openButton = New-Button $TextOpenWeb $layout.OpenX 3 80
    $deleteButton = New-Button $TextDeleteGateway $layout.DeleteX 3 100
    $openButton.Enabled = $false

    $row = [pscustomobject]@{
        Panel = $panel
        GatewayTextBox = $gatewayTextBox
        WebPortTextBox = $webPortTextBox
        ShellPortTextBox = $shellPortTextBox
        ToggleButton = $toggleButton
        OpenButton = $openButton
        DeleteButton = $deleteButton
        Process = $null
        OutputEventId = $null
        ErrorEventId = $null
        OutputJob = $null
        ErrorJob = $null
        VisitorConfigPath = $null
        Started = $false
        WebPort = $null
        ShellPort = $null
        PortGatewayId = $GatewayId
        SavedWebPort = $SavedWebPort
        SavedShellPort = $SavedShellPort
        PortLoadError = $portLoadError
    }

    $toggleButton.Tag = $row
    $openButton.Tag = $row
    $deleteButton.Tag = $row

    $toggleButton.Add_Click({
        try {
            $currentRow = $this.Tag
            if ($currentRow.Started) {
                Stop-GatewayRow $currentRow
            }
            else {
                Start-GatewayRow $currentRow
            }
        } catch { Show-ErrorMessage $_.Exception.Message }
    })

    $openButton.Add_Click({
        try { Open-GatewayWeb $this.Tag } catch { Show-ErrorMessage $_.Exception.Message }
    })

    $deleteButton.Add_Click({
        try {
            $currentRow = $this.Tag
            if ($currentRow.Started) { Stop-GatewayRow $currentRow }
            Remove-GatewayVisitorConfig $currentRow
            [void]$script:gatewayRows.Remove($currentRow)
            $gatewayPanel.Controls.Remove($currentRow.Panel)
            $currentRow.Panel.Dispose()
            if ($script:gatewayRows.Count -eq 0) { Add-GatewayRow '' }
            Save-RK3506LauncherConfig
            Append-Log 'Deleted gateway row.'
        } catch { Show-ErrorMessage $_.Exception.Message }
    })

    $panel.Controls.AddRange(@($gatewayTextBox, $webPortTextBox, $shellPortTextBox, $toggleButton, $openButton, $deleteButton))
    [void]$script:gatewayRows.Add($row)
    $gatewayPanel.Controls.Add($panel)
    Update-RK3506LauncherLayout
    return $row
}

function Stop-AllGatewayRows {
    $stopErrors = @()
    foreach ($row in @($script:gatewayRows)) {
        try {
            if ($row.Started) { Stop-GatewayRow $row }
        } catch {
            $stopErrors += "$($row.GatewayTextBox.Text.Trim()): $($_.Exception.Message)"
        }
    }
    if ($stopErrors.Count -gt 0) { throw "Could not stop every gateway visitor: $($stopErrors -join '; ')" }
}

$script:appConfig = Load-RK3506LauncherConfig

$form = New-Object System.Windows.Forms.Form
$form.Text = 'RK3506 Tunnel Launcher'
$form.Size = New-Object System.Drawing.Size(1060, 640)
$form.MinimumSize = New-Object System.Drawing.Size(980, 560)
$form.StartPosition = 'CenterScreen'

$addGatewayButton = New-Button $TextAddGateway 20 16 100

$gatewayHeaderPanel = New-Object System.Windows.Forms.Panel
$gatewayHeaderPanel.Location = New-Object System.Drawing.Point(20, 58)
$gatewayHeaderPanel.Size = New-Object System.Drawing.Size(920, 24)
$gatewayIdHeader = New-Label 'Gateway ID' 0 2 180
$webLinkHeader = New-Label 'Web Link' 195 2 300
$shellLinkHeader = New-Label 'Shell Link' 505 2 150
$actionHeader = New-Label 'Action' 670 2 120
$gatewayHeaderPanel.Controls.AddRange(@(
    $gatewayIdHeader,
    $webLinkHeader,
    $shellLinkHeader,
    $actionHeader
))

$gatewayPanel = New-Object System.Windows.Forms.FlowLayoutPanel
$gatewayPanel.Location = New-Object System.Drawing.Point(20, 84)
$gatewayPanel.Size = New-Object System.Drawing.Size(920, 210)
$gatewayPanel.FlowDirection = 'TopDown'
$gatewayPanel.WrapContents = $false
$gatewayPanel.AutoScroll = $true

$logLabel = New-Label 'Log' 20 304 100

$logTextBox = New-Object System.Windows.Forms.TextBox
$logTextBox.Location = New-Object System.Drawing.Point(20, 328)
$logTextBox.Size = New-Object System.Drawing.Size(920, 230)
$logTextBox.Multiline = $true
$logTextBox.ScrollBars = 'Vertical'
$logTextBox.ReadOnly = $true

$form.Controls.AddRange(@(
    $addGatewayButton,
    $gatewayHeaderPanel,
    $gatewayPanel,
    $logLabel,
    $logTextBox
))

$addGatewayButton.Add_Click({
    try {
        Add-GatewayRow '' | Out-Null
        Save-RK3506LauncherConfig
    } catch { Show-ErrorMessage $_.Exception.Message }
})

foreach ($gateway in @($script:appConfig.gateways)) {
    if ($gateway.PSObject.Properties['gatewayId']) {
        Add-GatewayRow -GatewayId ([string]$gateway.gatewayId) `
            -SavedWebPort $gateway.webBindPort -SavedShellPort $gateway.sshBindPort | Out-Null
    }
}
if ($script:gatewayRows.Count -eq 0) {
    Add-GatewayRow '' | Out-Null
}

$form.Add_FormClosing({
    param($sender, $eventArgs)
    try { Save-RK3506LauncherConfig } catch { }
    try { Stop-AllGatewayRows } catch {
        $eventArgs.Cancel = $true
        Show-ErrorMessage $_.Exception.Message
    }
})

$form.Add_Resize({ Update-RK3506LauncherLayout })

Update-RK3506LauncherLayout

Append-Log 'Launcher ready. Add gateways, then start each row as needed.'
[void]$form.ShowDialog()
