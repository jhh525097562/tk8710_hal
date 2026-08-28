param(
    [int]$ListenPort = 20165,
    [int]$TimeoutSeconds = 3,
    [int]$Retries = 3,
    [string[]]$ScanSubnets = @(
        '192.168.100.0/24',
        '192.168.200.0/24'
    ),
    [int]$MaxUnicastHosts = 4096
)

$ErrorActionPreference = 'Stop'

function Get-DirectedBroadcast {
    param(
        [Parameter(Mandatory = $true)][System.Net.IPAddress]$Address,
        [Parameter(Mandatory = $true)][ValidateRange(0, 32)][int]$PrefixLength
    )

    $ipBytes = $Address.GetAddressBytes()
    $broadcastBytes = New-Object byte[] 4

    for ($index = 0; $index -lt 4; $index++) {
        $remainingBits = $PrefixLength - ($index * 8)
        if ($remainingBits -ge 8) {
            $maskByte = 255
        }
        elseif ($remainingBits -le 0) {
            $maskByte = 0
        }
        else {
            $maskByte = (256 - [math]::Pow(2, 8 - $remainingBits))
        }

        $broadcastBytes[$index] = $ipBytes[$index] -bor (255 - [int]$maskByte)
    }

    return [System.Net.IPAddress]::new($broadcastBytes)
}

function ConvertTo-IPv4UInt32 {
    param([Parameter(Mandatory = $true)][System.Net.IPAddress]$Address)

    $bytes = $Address.GetAddressBytes()
    return [uint32](
        ([uint32]$bytes[0] -shl 24) -bor
        ([uint32]$bytes[1] -shl 16) -bor
        ([uint32]$bytes[2] -shl 8) -bor
        [uint32]$bytes[3]
    )
}

function ConvertFrom-IPv4UInt32 {
    param([Parameter(Mandatory = $true)][uint32]$Value)

    $bytes = [byte[]]@(
        (($Value -shr 24) -band 255),
        (($Value -shr 16) -band 255),
        (($Value -shr 8) -band 255),
        ($Value -band 255)
    )
    return [System.Net.IPAddress]::new($bytes)
}

function Get-SubnetHosts {
    param(
        [Parameter(Mandatory = $true)][string]$Cidr,
        [Parameter(Mandatory = $true)][int]$MaximumHosts
    )

    if ($Cidr -notmatch '^([^/]+)/([0-9]|[12][0-9]|3[0-2])$') {
        throw "Invalid IPv4 CIDR: $Cidr"
    }

    $address = $null
    if (-not [System.Net.IPAddress]::TryParse($Matches[1], [ref]$address) -or
        $address.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) {
        throw "Invalid IPv4 CIDR: $Cidr"
    }

    $prefix = [int]$Matches[2]
    $hostBits = 32 - $prefix
    $addressCount = [uint64]1 -shl $hostBits
    $usableCount = if ($prefix -le 30) { $addressCount - 2 } else { $addressCount }
    if ($usableCount -gt $MaximumHosts) {
        throw "Subnet $Cidr contains $usableCount hosts; limit is $MaximumHosts. Use a smaller CIDR or raise -MaxUnicastHosts."
    }

    $ipValue = ConvertTo-IPv4UInt32 -Address $address
    $mask = if ($prefix -eq 0) { [uint32]0 } else { [uint32]::MaxValue -shl $hostBits }
    $network = [uint32]($ipValue -band $mask)
    $firstOffset = if ($prefix -le 30) { 1 } else { 0 }
    $lastOffset = if ($prefix -le 30) { $addressCount - 2 } else { $addressCount - 1 }

    for ($offset = [uint64]$firstOffset; $offset -le $lastOffset; $offset++) {
        ConvertFrom-IPv4UInt32 -Value ([uint32]([uint64]$network + $offset))
    }
}

function Receive-GatewayReplies {
    param(
        [Parameter(Mandatory = $true)][System.Net.Sockets.UdpClient]$Client,
        [Parameter(Mandatory = $true)][datetime]$Deadline,
        [Parameter(Mandatory = $true)][hashtable]$Results,
        [Parameter(Mandatory = $true)][string]$LocalAddress
    )

    while ([datetime]::UtcNow -lt $Deadline) {
        try {
            $remote = [System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0)
            $bytes = $Client.Receive([ref]$remote)
            $text = [System.Text.Encoding]::UTF8.GetString($bytes)
            $reply = $text | ConvertFrom-Json

            if ($reply.cmd -ne 'discovery_rsp') {
                continue
            }

            $key = if ($reply.gwid) {
                [string]$reply.gwid
            }
            elseif ($reply.mac) {
                [string]$reply.mac
            }
            else {
                [string]$remote.Address
            }

            $Results[$key] = [pscustomobject]@{
                GatewayId      = [string]$reply.gwid
                ReportedIP     = [string]$reply.ip
                SourceIP       = [string]$remote.Address
                MAC            = [string]$reply.mac
                ActiveInterface = [string]$reply.active_iface
                ReceivedVia    = $LocalAddress
            }
        }
        catch [System.Management.Automation.RuntimeException] {
            Write-Warning "Invalid discovery response: $($_.Exception.Message)"
        }
        catch [System.Net.Sockets.SocketException] {
            if ($_.Exception.SocketErrorCode -notin @(
                    [System.Net.Sockets.SocketError]::TimedOut,
                    [System.Net.Sockets.SocketError]::WouldBlock)) {
                Write-Warning "Failed to receive discovery response: $($_.Exception.Message)"
            }
        }
    }
}

$requestText = '{"cmd":"discovery_req","magic":"turmass_link"}'
$requestBytes = [System.Text.Encoding]::UTF8.GetBytes($requestText)
$results = @{}

$interfaces = @(
    Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop |
        Where-Object {
            $_.IPAddress -ne '127.0.0.1' -and
            $_.IPAddress -notlike '169.254.*' -and
            $_.AddressState -eq 'Preferred'
        } |
        Sort-Object InterfaceIndex, IPAddress -Unique
)

if ($interfaces.Count -eq 0) {
    throw 'No usable IPv4 network interface was found.'
}

Write-Host "Searching for gateways through $($interfaces.Count) IPv4 interface(s), UDP port $ListenPort ..." -ForegroundColor Cyan

foreach ($interface in $interfaces) {
    $localIP = [System.Net.IPAddress]::Parse($interface.IPAddress)
    $directedBroadcast = Get-DirectedBroadcast -Address $localIP -PrefixLength $interface.PrefixLength
    $targets = @($directedBroadcast, [System.Net.IPAddress]::Broadcast) |
        Select-Object -Unique

    Write-Host "Interface: $($interface.InterfaceAlias)  $localIP/$($interface.PrefixLength)  Broadcast: $directedBroadcast"

    $client = [System.Net.Sockets.UdpClient]::new()
    try {
        $client.EnableBroadcast = $true
        $client.Client.ReceiveTimeout = 250
        $client.Client.Bind([System.Net.IPEndPoint]::new($localIP, 0))

        for ($attempt = 1; $attempt -le $Retries; $attempt++) {
            foreach ($target in $targets) {
                $destination = [System.Net.IPEndPoint]::new($target, $ListenPort)
                [void]$client.Send($requestBytes, $requestBytes.Length, $destination)
            }
            Start-Sleep -Milliseconds 100
        }

        $deadline = [datetime]::UtcNow.AddSeconds($TimeoutSeconds)
        Receive-GatewayReplies -Client $client -Deadline $deadline -Results $results -LocalAddress ([string]$localIP)
    }
    catch {
        Write-Warning "Search failed on interface $localIP : $($_.Exception.Message)"
    }
    finally {
        $client.Close()
    }
}

foreach ($subnet in $ScanSubnets) {
    if ([string]::IsNullOrWhiteSpace($subnet)) {
        continue
    }

    $hosts = @(Get-SubnetHosts -Cidr $subnet.Trim() -MaximumHosts $MaxUnicastHosts)
    Write-Host "Unicast scan: $subnet ($($hosts.Count) host(s))"

    $client = [System.Net.Sockets.UdpClient]::new()
    try {
        $client.Client.ReceiveTimeout = 200
        $client.Client.Bind([System.Net.IPEndPoint]::new([System.Net.IPAddress]::Any, 0))

        foreach ($hostAddress in $hosts) {
            $destination = [System.Net.IPEndPoint]::new($hostAddress, $ListenPort)
            try {
                [void]$client.Send($requestBytes, $requestBytes.Length, $destination)
            }
            catch [System.Net.Sockets.SocketException] {
                # An unreachable routed subnet is reported after the receive phase.
            }
        }

        $deadline = [datetime]::UtcNow.AddSeconds($TimeoutSeconds)
        Receive-GatewayReplies -Client $client -Deadline $deadline -Results $results -LocalAddress "route:$subnet"
    }
    finally {
        $client.Close()
    }
}

Write-Host ''
if ($results.Count -eq 0) {
    Write-Host 'No 880 gateway was discovered.' -ForegroundColor Yellow
    Write-Host 'Check lan_discoveryd, UDP 20165, firewall rules, broadcast forwarding, and the unicast return route.'
    exit 1
}

Write-Host "Discovered $($results.Count) gateway(s):" -ForegroundColor Green
$results.Values |
    Sort-Object GatewayId, ReportedIP |
    Format-Table GatewayId, ReportedIP, SourceIP, MAC, ActiveInterface, ReceivedVia -AutoSize

exit 0
