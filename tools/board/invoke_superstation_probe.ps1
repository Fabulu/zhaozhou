[CmdletBinding(DefaultParameterSetName = 'Probe')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Probe')]
    [string]$RbfPath,

    [Parameter(Mandatory = $true, ParameterSetName = 'Rollback')]
    [switch]$RehearseRollback,

    [Parameter(Mandatory = $true, ParameterSetName = 'Identity')]
    [switch]$IdentityPreflight,

    [switch]$Execute,

    [ValidateSet('Bringup', 'Specs')]
    [string]$Profile = 'Bringup',

    [string]$HostName = '192.168.178.59',
    [string]$UserName = 'root',
    [string]$AskPassPath,
    [ValidateRange(5, 120)]
    [int]$HoldSeconds = 20,
    [Parameter(ParameterSetName = 'Probe')]
    [switch]$ExerciseWatchdog,
    [string]$ReceiptPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$expectedCheckout = 'zhaozhou-board-bringup-20260913'
$expectedBranch = 'zhaozhou-board-bringup-20260913'
$menuPath = '/media/fat/menu.rbf'
$menuSha256 = '25d5461b55e4d45e79c876a02d69f32b22f414b64e600a1adc930eefea6ea4a7'
$expectedSshEd25519 = 'SHA256:FqNJOsj3FLUoMQxgn+cqGoXvVfENmVK4QFoSCMKl2lU'
$expectedBoardIdentity = [ordered]@{
    hostname = 'MiSTer'
    model = 'Terasic DE10-nano'
    compatible = 'altr,socfpga-cyclone5 altr,socfpga'
    ethernetMac = 'ce:cd:87:14:8d:44'
    hpsSiliconId1 = '0x00000003'
    misterSha256 = '9f6e5a237c36be6404ab4823d804821491db4bf125827f84aca2a1ca31f0a8a6'
    menuSha256 = $menuSha256
}
$target = "$UserName@$HostName"
$profileConfig = if ($Profile -eq 'Specs') {
    [ordered]@{
        project = 'ZhaozhouSpecs'
        expectedCore = 'Zhaozhou Hardware Specs'
        verifier = 'tools\board\verify_superstation_specs.py'
        audit = 'runs\CLAUDE-RUNS\RUN-20260913-1651-board-bringup\HARDWARE-SPECS-BUILD-AUDIT-V2.json'
        manifest = 'runs\CLAUDE-RUNS\RUN-20260913-1651-board-bringup\HARDWARE-SPECS-BUILD-MANIFEST-V2.json'
        marker = '.zhaozhou-superstation-specs-build'
        remotePrefix = 'ZhaozhouSpecs'
        receipt = 'HARDWARE-SPECS-LOAD.json'
        sourcePaths = @(
            'fpga/sys',
            'fpga/ZhaozhouSpecs.qpf',
            'fpga/ZhaozhouSpecs.qsf',
            'fpga/ZhaozhouSpecs.sdc',
            'fpga/files_specs.qip',
            'fpga/rtl/common/zhao_crc32c_fold.sv',
            'fpga/rtl/raster/zhao_raster_fill.sv',
            'fpga/rtl/common/zhao_dual18_mul.sv',
            'fpga/rtl/platform/zhao_ssone_spec_tests.sv',
            'fpga/rtl/platform/zhao_ssone_specs_emu.sv',
            'fpga/rtl/pll.qip',
            'fpga/rtl/pll.v',
            'fpga/rtl/pll'
        )
    }
} else {
    [ordered]@{
        project = 'ZhaozhouBringup'
        expectedCore = 'Zhaozhou Board Bring-up'
        verifier = 'tools\board\verify_superstation_bringup.py'
        audit = 'runs\CLAUDE-RUNS\RUN-20260913-1651-board-bringup\BRINGUP-BUILD-AUDIT-V2.json'
        manifest = 'runs\CLAUDE-RUNS\RUN-20260913-1651-board-bringup\BRINGUP-BUILD-MANIFEST-V2.json'
        marker = '.zhaozhou-superstation-build'
        remotePrefix = 'ZhaozhouBringup'
        receipt = 'FIRST-VOLATILE-LOAD.json'
        sourcePaths = @(
            'fpga/sys',
            'fpga/ZhaozhouBringup.qpf',
            'fpga/ZhaozhouBringup.qsf',
            'fpga/ZhaozhouBringup.sdc',
            'fpga/files_bringup.qip',
            'fpga/rtl/platform/zhao_ssone_bringup.sv',
            'fpga/rtl/pll.qip',
            'fpga/rtl/pll.v',
            'fpga/rtl/pll'
        )
    }
}

if (-not $IdentityPreflight -and -not $Execute) {
    throw 'This tool changes the live FPGA image. Pass -Execute only after reviewing the build and rollback receipt.'
}
if ($UserName -ne 'root') {
    throw "Pinned board identity requires SSH user root, not '$UserName'"
}
if ($PSCmdlet.ParameterSetName -eq 'Probe' -and -not $ExerciseWatchdog) {
    throw 'Physical probe loads require -ExerciseWatchdog so fired/write/MENU evidence is captured.'
}
if ($PSCmdlet.ParameterSetName -eq 'Probe' -and [string]::IsNullOrWhiteSpace($ReceiptPath)) {
    throw 'Physical probe loads require a new explicit -ReceiptPath; historical receipts are never overwritten.'
}
if ($ReceiptPath) {
    $ReceiptPath = [IO.Path]::GetFullPath($ReceiptPath)
    if (Test-Path -LiteralPath $ReceiptPath) {
        throw "Refusing to overwrite existing receipt: $ReceiptPath"
    }
    $receiptParent = Split-Path $ReceiptPath -Parent
    if (-not (Test-Path -LiteralPath $receiptParent -PathType Container)) {
        throw "Receipt parent directory does not exist: $receiptParent"
    }
}
if ((Split-Path $repoRoot -Leaf) -ne $expectedCheckout) {
    throw "Board load must run from the dedicated $expectedCheckout checkout, not $repoRoot"
}
$branch = (& git -C $repoRoot branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or $branch -ne $expectedBranch) {
    throw "Board load requires branch $expectedBranch; current branch is '$branch'"
}

if ($AskPassPath) {
    $AskPassPath = (Resolve-Path $AskPassPath).Path
    if (-not (Test-Path -LiteralPath $AskPassPath -PathType Leaf)) {
        throw "SSH askpass helper not found: $AskPassPath"
    }
}

$sshOptions = @()
$scpOptions = @()
$knownHostsFile = $null

function Initialize-PinnedSsh {
    if ($HostName -notmatch '^[A-Za-z0-9.-]+$') { throw "Unsafe SSH host name: $HostName" }
    $scanner = (Get-Command ssh-keyscan.exe -ErrorAction Stop).Source
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $scanner
    $start.Arguments = "-T 5 -t ed25519 $HostName"
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    if (-not $process.Start()) { throw "Could not start ssh-keyscan for $HostName" }
    $stdout = $process.StandardOutput.ReadToEnd()
    $stderr = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    $scanRc = $process.ExitCode
    $process.Dispose()
    if ($scanRc -ne 0) {
        throw "ssh-keyscan failed for $HostName with exit code ${scanRc}: $stderr"
    }
    $scan = @($stdout -split "`r?`n" | Where-Object { $_ })
    $keys = @($scan | Where-Object { $_ -match '^\S+\s+ssh-ed25519\s+[A-Za-z0-9+/=]+$' })
    if ($keys.Count -ne 1) {
        throw "Expected exactly one ED25519 host key for $HostName, got $($keys.Count)"
    }
    $parts = $keys[0] -split '\s+'
    $blob = [Convert]::FromBase64String($parts[2])
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        $digest = $sha.ComputeHash($blob)
    } finally {
        $sha.Dispose()
    }
    $fingerprint = 'SHA256:' + [Convert]::ToBase64String($digest).TrimEnd('=')
    if ($fingerprint -ne $expectedSshEd25519) {
        throw "SSH host fingerprint mismatch: expected $expectedSshEd25519, got $fingerprint"
    }

    $file = [IO.Path]::GetTempFileName()
    [IO.File]::WriteAllText(
        $file,
        $keys[0] + "`n",
        (New-Object Text.UTF8Encoding($false))
    )
    return [ordered]@{
        fingerprint = $fingerprint
        algorithm = 'ssh-ed25519'
        keyLine = $keys[0]
        knownHostsFile = $file
    }
}

function Invoke-BoardSsh {
    param([Parameter(Mandatory = $true)][string]$Command)
    $output = @(& ssh.exe @sshOptions $target $Command)
    if ($LASTEXITCODE -ne 0) {
        throw "SSH command failed with exit code ${LASTEXITCODE}: $Command"
    }
    return $output
}

function Convert-ExactKeyValueLines {
    param(
        [Parameter(Mandatory = $true)][string[]]$Lines,
        [Parameter(Mandatory = $true)][string[]]$ExpectedKeys,
        [Parameter(Mandatory = $true)][string]$Context
    )
    $result = [ordered]@{}
    foreach ($line in $Lines) {
        if ($line -notmatch '^([^=]+)=(.*)$') {
            throw "$Context emitted a non-key/value line: $line"
        }
        $key = $Matches[1]
        if ($result.Contains($key)) { throw "$Context emitted duplicate key: $key" }
        $result[$key] = $Matches[2]
    }
    $missing = @($ExpectedKeys | Where-Object { -not $result.Contains($_) })
    $extra = @($result.Keys | Where-Object { $_ -notin $ExpectedKeys })
    if ($missing.Count -ne 0 -or $extra.Count -ne 0) {
        throw "$Context key mismatch: missing=$($missing -join ',') extra=$($extra -join ',')"
    }
    return $result
}

function Get-BoardIdentity {
    $command = @'
printf 'hostname='; hostname
printf 'model='; tr '\000' ' ' < /proc/device-tree/model | xargs
printf 'compatible='; tr '\000' ' ' < /proc/device-tree/compatible | xargs
printf 'ethernetMac='; cat /sys/class/net/eth0/address
printf 'hpsSiliconId1='; devmem 0xffd08000 32
printf 'misterSha256='; sha256sum /media/fat/MiSTer | cut -d' ' -f1
printf 'menuSha256='; sha256sum /media/fat/menu.rbf | cut -d' ' -f1
'@
    return @(Invoke-BoardSsh $command)
}

function Assert-BoardIdentity {
    param([Parameter(Mandatory = $true)][string[]]$Lines)
    $expectedKeys = @($expectedBoardIdentity.Keys)
    $actual = Convert-ExactKeyValueLines $Lines $expectedKeys 'board identity'
    foreach ($key in $expectedKeys) {
        if ($actual[$key] -ne $expectedBoardIdentity[$key]) {
            throw "Board identity mismatch for ${key}: expected '$($expectedBoardIdentity[$key])', got '$($actual[$key])'"
        }
    }
    return $actual
}

function Assert-BoardState {
    param(
        [Parameter(Mandatory = $true)][string[]]$Lines,
        [Parameter(Mandatory = $true)][string]$ExpectedCore,
        [Parameter(Mandatory = $true)][string]$Context
    )
    $expectedKeys = @(
        'utc', 'core', 'rbf', 'fpga',
        'bridge:lwhps2fpga', 'bridge:hps2fpga', 'bridge:fpga2hps',
        'mister_pid'
    )
    $state = Convert-ExactKeyValueLines $Lines $expectedKeys $Context
    if ($state.utc -notmatch '^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$') {
        throw "$Context UTC timestamp is malformed: $($state.utc)"
    }
    if ($state.core -cne $ExpectedCore -or $state.rbf -cne $ExpectedCore) {
        throw "$Context core identity mismatch: core='$($state.core)' rbf='$($state.rbf)' expected='$ExpectedCore'"
    }
    if ($state.fpga -cne 'operating') {
        throw "$Context FPGA state is '$($state.fpga)', expected 'operating'"
    }
    foreach ($bridge in @('lwhps2fpga', 'hps2fpga', 'fpga2hps')) {
        if ($state["bridge:$bridge"] -cne 'enabled') {
            throw "$Context bridge $bridge is '$($state["bridge:$bridge"])', expected 'enabled'"
        }
    }
    if ($state.mister_pid -notmatch '^\d+$') {
        throw "$Context MiSTer PID is not exactly one numeric process: '$($state.mister_pid)'"
    }
    return $state
}

function Get-BoardState {
    $command = @'
printf 'utc='; date -u +%Y-%m-%dT%H:%M:%SZ
printf 'core='; cat /tmp/CORENAME 2>/dev/null || true; printf '\n'
printf 'rbf='; cat /tmp/RBFNAME 2>/dev/null || true; printf '\n'
printf 'fpga='; cat /sys/class/fpga_manager/fpga0/state
for d in /sys/class/fpga_bridge/*; do printf 'bridge:%s=' "$(cat "$d/name")"; cat "$d/state"; done
printf 'mister_pid='; pidof MiSTer
'@
    return @(Invoke-BoardSsh $command)
}

function Set-BoardCore {
    param([Parameter(Mandatory = $true)][string]$RemotePath)
    if ($RemotePath -notmatch '^/media/fat/[A-Za-z0-9_./-]+$') {
        throw "Unsafe remote core path: $RemotePath"
    }
    Invoke-BoardSsh "printf '%s\n' 'load_core $RemotePath' > /dev/MiSTer_cmd" | Out-Null
}

function Arm-BoardRollbackWatchdog {
    param(
        [Parameter(Mandatory = $true)][string]$Identifier,
        [Parameter(Mandatory = $true)][int]$DelaySeconds
    )
    if ($Identifier -notmatch '^[0-9a-f]{12}$') { throw "Unsafe watchdog identifier: $Identifier" }
    $token = "/tmp/zhaozhou-rollback-$Identifier.armed"
    $log = "/tmp/zhaozhou-rollback-$Identifier.log"
    $script = "/tmp/zhaozhou-rollback-$Identifier.sh"
    $scriptTemplate = @'
#!/bin/sh
sleep __DELAY__
if [ ! -e "__TOKEN__" ]; then
    exit 0
fi
date -u +watchdog-fired=%Y-%m-%dT%H:%M:%SZ
attempt=1
while [ "$attempt" -le 5 ] && [ -e "__TOKEN__" ]; do
    echo "watchdog-attempt=$attempt"
    if timeout 3 sh -c 'printf "%s\n" "load_core __MENU__" > /dev/MiSTer_cmd'; then
        echo "watchdog-write-ok=$attempt"
        sleep 4
        if [ "$(cat /tmp/CORENAME 2>/dev/null)" = "MENU" ]; then
            echo "watchdog-menu-ok=$attempt"
            exit 0
        fi
    else
        echo "watchdog-write-timeout=$attempt"
    fi
    attempt=$((attempt + 1))
    sleep 2
done
echo watchdog-exhausted
exit 1
'@
    $scriptText = $scriptTemplate.Replace('__DELAY__', [string]$DelaySeconds).
                                  Replace('__TOKEN__', $token).
                                  Replace('__MENU__', $menuPath)
    $encodedScript = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($scriptText))
    $command = "rm -f $token $log $script; " +
               "printf '%s' '$encodedScript' | base64 -d > $script; chmod 700 $script; : > $token; " +
               "nohup $script >$log 2>&1 </dev/null & echo `$!"
    $watchdogPid = @(Invoke-BoardSsh $command)[-1]
    if ($watchdogPid -notmatch '^\d+$') { throw "HPS rollback watchdog returned invalid PID: $watchdogPid" }
    Invoke-BoardSsh "test -e $token && test -x $script && kill -0 $watchdogPid" | Out-Null
    return [ordered]@{
        pid = [int]$watchdogPid
        token = $token
        log = $log
        script = $script
        delaySeconds = $DelaySeconds
    }
}

function Disarm-BoardRollbackWatchdog {
    param([Parameter(Mandatory = $true)][System.Collections.IDictionary]$Watchdog)
    $watchdogPid = $Watchdog.pid
    $token = $Watchdog.token
    $log = $Watchdog.log
    $script = $Watchdog.script
    $lines = @(Invoke-BoardSsh "rm -f $token; kill $watchdogPid 2>/dev/null || true; cat $log 2>/dev/null || true; rm -f $log $script")
    return $lines
}

function Assert-WatchdogEvidence {
    param([Parameter(Mandatory = $true)][string[]]$Lines)
    if ($Lines | Where-Object { $_ -eq 'watchdog-exhausted' }) {
        throw 'HPS watchdog exhausted all FIFO attempts.'
    }
    $fired = @($Lines | Where-Object { $_ -match '^watchdog-fired=\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$' })
    $attemptLines = @($Lines | Where-Object { $_ -match '^watchdog-attempt=(\d+)$' })
    $writeLines = @($Lines | Where-Object { $_ -match '^watchdog-write-ok=(\d+)$' })
    $menuLines = @($Lines | Where-Object { $_ -match '^watchdog-menu-ok=(\d+)$' })
    if ($fired.Count -ne 1 -or $writeLines.Count -ne 1 -or $menuLines.Count -ne 1) {
        throw "Incomplete HPS watchdog evidence: fired=$($fired.Count) write=$($writeLines.Count) menu=$($menuLines.Count)"
    }
    $writeAttempt = [int]($writeLines[0] -replace '^watchdog-write-ok=', '')
    $menuAttempt = [int]($menuLines[0] -replace '^watchdog-menu-ok=', '')
    if ($writeAttempt -ne $menuAttempt) {
        throw "HPS watchdog write/MENU attempt mismatch: $writeAttempt != $menuAttempt"
    }
    $attempts = @($attemptLines | ForEach-Object { [int]($_ -replace '^watchdog-attempt=', '') })
    $expectedAttempts = @(1..$writeAttempt)
    if (($attempts -join ',') -ne ($expectedAttempts -join ',')) {
        throw "HPS watchdog attempts are not contiguous through success: '$($attempts -join ',')'"
    }
    if ($writeAttempt -gt 1) {
        foreach ($attempt in 1..($writeAttempt - 1)) {
            if ($Lines -notcontains "watchdog-write-timeout=$attempt") {
                throw "HPS watchdog attempt $attempt has neither timeout nor terminal success evidence"
            }
        }
    }
    return [ordered]@{
        firedUtc = $fired[0].Substring('watchdog-fired='.Length)
        attempts = $attempts
        successfulWriteAttempt = $writeAttempt
        menuVerifiedAttempt = $menuAttempt
    }
}

function Restore-EnvironmentValue {
    param([string]$Name, [AllowNull()][string]$Value)
    if ($null -eq $Value) {
        Remove-Item "Env:$Name" -ErrorAction SilentlyContinue
    } else {
        Set-Item "Env:$Name" $Value
    }
}

$oldAskPass = $env:SSH_ASKPASS
$oldAskPassRequire = $env:SSH_ASKPASS_REQUIRE
$oldDisplay = $env:DISPLAY
if ($AskPassPath) {
    $env:SSH_ASKPASS = $AskPassPath
    $env:SSH_ASKPASS_REQUIRE = 'force'
    $env:DISPLAY = 'zhaozhou-board-bringup'
}

$started = (Get-Date).ToUniversalTime().ToString('o')
$receipt = [ordered]@{
    schema = 'zhaozhou.superstation.load-receipt.v1'
    startedUtc = $started
    host = $HostName
    branch = $branch
    profile = $Profile
    sourceCommit = (& git -C $repoRoot rev-parse HEAD).Trim()
    buildSourceCommit = $null
    buildAudit = $null
    buildManifest = $null
    sshHostKey = $null
    boardIdentity = $null
    mode = if ($IdentityPreflight) { 'identity-preflight' } elseif ($RehearseRollback) { 'rollback-rehearsal' } elseif ($ExerciseWatchdog) { 'watchdog-fire-test' } else { 'probe-transaction' }
    before = @()
    beforeState = $null
    loaded = @()
    loadedState = $null
    afterRollback = @()
    afterRollbackState = $null
    localRbf = $null
    remoteRbf = $null
    menuRbf = [ordered]@{ path = $menuPath; expectedSha256 = $menuSha256; observedSha256 = $null }
    watchdog = $null
    watchdogLog = @()
    watchdogEvidence = $null
    rollbackAttempted = $false
    rollbackSucceeded = $false
    stagedFileRemoved = $false
    status = 'started'
    error = $null
}

$probeLoadAttempted = $false
$watchdogRollbackObserved = $false
$createdRemoteFile = $false
$remotePath = $null
$watchdog = $null
$pendingError = $null

try {
    $sshPin = Initialize-PinnedSsh
    $knownHostsFile = $sshPin.knownHostsFile
    $sshOptions = @(
        '-T',
        '-o', 'ConnectTimeout=8',
        '-o', 'ConnectionAttempts=1',
        '-o', 'StrictHostKeyChecking=yes',
        '-o', 'HostKeyAlgorithms=ssh-ed25519',
        '-o', 'UpdateHostKeys=no',
        '-o', "UserKnownHostsFile=$knownHostsFile",
        '-o', "GlobalKnownHostsFile=$knownHostsFile",
        '-o', 'NumberOfPasswordPrompts=1'
    )
    $scpOptions = @(
        '-q',
        '-o', 'ConnectTimeout=8',
        '-o', 'ConnectionAttempts=1',
        '-o', 'StrictHostKeyChecking=yes',
        '-o', 'HostKeyAlgorithms=ssh-ed25519',
        '-o', 'UpdateHostKeys=no',
        '-o', "UserKnownHostsFile=$knownHostsFile",
        '-o', "GlobalKnownHostsFile=$knownHostsFile",
        '-o', 'NumberOfPasswordPrompts=1'
    )
    $receipt.sshHostKey = [ordered]@{
        host = $HostName
        algorithm = $sshPin.algorithm
        fingerprint = $sshPin.fingerprint
    }
    $identityLines = @(Get-BoardIdentity)
    $receipt.boardIdentity = Assert-BoardIdentity $identityLines

    $receipt.before = @(Get-BoardState)
    $receipt.beforeState = Assert-BoardState $receipt.before 'MENU' 'before load'
    Invoke-BoardSsh "test -p /dev/MiSTer_cmd && test -r $menuPath" | Out-Null
    $observedMenuHash = @(Invoke-BoardSsh "sha256sum $menuPath | cut -d' ' -f1")[-1]
    $receipt.menuRbf.observedSha256 = $observedMenuHash
    if ($observedMenuHash -ne $menuSha256) {
        throw "Rollback menu hash mismatch: expected $menuSha256, got $observedMenuHash"
    }

    if ($IdentityPreflight) {
        $receipt.status = 'ok'
    } elseif ($RehearseRollback) {
        $receipt.rollbackAttempted = $true
        Set-BoardCore $menuPath
        Start-Sleep -Seconds 4
        $receipt.afterRollback = @(Get-BoardState)
        $receipt.afterRollbackState = Assert-BoardState $receipt.afterRollback 'MENU' 'rollback rehearsal'
        $receipt.rollbackSucceeded = $true
        $receipt.status = 'ok'
    } else {
        $localRbfPath = (Resolve-Path $RbfPath).Path
        if (-not (Test-Path -LiteralPath $localRbfPath -PathType Leaf)) {
            throw "RBF not found: $localRbfPath"
        }
        $buildRoot = Split-Path (Split-Path $localRbfPath -Parent) -Parent
        $buildMarker = Join-Path $buildRoot $profileConfig.marker
        if (-not (Test-Path -LiteralPath $buildMarker -PathType Leaf)) {
            throw "RBF is not in an owned SuperStation build directory: $buildRoot"
        }
        $markerText = [System.IO.File]::ReadAllText($buildMarker)
        $markerMatch = [regex]::Match($markerText, '(?m)^sourceCommit=([0-9a-f]{40})$')
        if (-not $markerMatch.Success) {
            throw "Build marker has no valid sourceCommit: $buildMarker"
        }
        $buildSourceCommit = $markerMatch.Groups[1].Value
        $receipt.buildSourceCommit = $buildSourceCommit
        & git -C $repoRoot cat-file -e "$buildSourceCommit^{commit}"
        if ($LASTEXITCODE -ne 0) { throw "Build source commit is not present: $buildSourceCommit" }
        $rbfSourcePaths = $profileConfig.sourcePaths
        & git -C $repoRoot diff --quiet $buildSourceCommit -- $rbfSourcePaths
        $sourceDiffRc = $LASTEXITCODE
        if ($sourceDiffRc -eq 1) {
            throw "Board sources changed after build commit $buildSourceCommit; rebuild before loading."
        }
        if ($sourceDiffRc -ne 0) {
            throw "Could not compare current board sources with build commit $buildSourceCommit"
        }
        if ((Split-Path $localRbfPath -Leaf) -ne "$($profileConfig.project).rbf") {
            throw "RBF name does not match profile $Profile`: $localRbfPath"
        }
        $verifyScript = Join-Path $repoRoot $profileConfig.verifier
        & python $verifyScript --repo $repoRoot --build-dir $buildRoot
        if ($LASTEXITCODE -ne 0) { throw 'Local RBF/build verification failed.' }

        $localHash = (Get-FileHash -LiteralPath $localRbfPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $localBytes = (Get-Item -LiteralPath $localRbfPath).Length

        $manifestPath = Join-Path $repoRoot $profileConfig.manifest
        if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
            throw "Committed complete build manifest not found: $manifestPath"
        }
        $manifestScript = Join-Path $repoRoot 'tools\board\superstation_build_manifest.py'
        & python $manifestScript verify --manifest $manifestPath --repo $repoRoot --build-dir $buildRoot
        if ($LASTEXITCODE -ne 0) { throw 'Complete source/report/RBF manifest verification failed.' }
        $manifest = [System.IO.File]::ReadAllText($manifestPath) | ConvertFrom-Json
        if ($manifest.status -ne 'candidate' -or
            $manifest.profile -ne $Profile -or
            $manifest.project -ne $profileConfig.project -or
            $manifest.sourceCommit -ne $buildSourceCommit) {
            throw "Complete build manifest does not authorize profile $Profile at $buildSourceCommit"
        }
        $manifestRbfName = "output_files/$($profileConfig.project).rbf"
        $manifestRbfProperty = $manifest.artifacts.PSObject.Properties[$manifestRbfName]
        if ($null -eq $manifestRbfProperty) { throw "Complete build manifest has no $manifestRbfName" }
        if ($manifestRbfProperty.Value.sha256 -ne $localHash -or
            [int64]$manifestRbfProperty.Value.bytes -ne $localBytes) {
            throw "Local RBF differs from complete build manifest: $localRbfPath"
        }
        $receipt.buildManifest = [ordered]@{
            path = $manifestPath
            manifestSha256 = $manifest.manifestSha256
            status = $manifest.status
            sourceCommit = $manifest.sourceCommit
            rbfSha256 = $manifestRbfProperty.Value.sha256
        }

        $auditPath = Join-Path $repoRoot $profileConfig.audit
        if (-not (Test-Path -LiteralPath $auditPath -PathType Leaf)) {
            throw "Committed Quartus audit not found: $auditPath"
        }
        $audit = [System.IO.File]::ReadAllText($auditPath) | ConvertFrom-Json
        if ($audit.status -ne 'ok' -or $audit.sourceCommit -ne $buildSourceCommit) {
            throw "Quartus audit does not authorize build commit $buildSourceCommit"
        }
        $artifactName = "$($profileConfig.project).rbf"
        $artifactProperty = $audit.artifacts.PSObject.Properties[$artifactName]
        if ($null -eq $artifactProperty) { throw "Quartus audit has no $artifactName artifact" }
        $auditedRbf = $artifactProperty.Value
        if ($auditedRbf.sha256 -ne $localHash -or [int64]$auditedRbf.bytes -ne $localBytes) {
            throw "Local RBF differs from committed Quartus audit: $localRbfPath"
        }
        $receipt.buildAudit = [ordered]@{
            path = $auditPath
            status = $audit.status
            sourceCommit = $audit.sourceCommit
            rbfSha256 = $auditedRbf.sha256
        }
        $remotePath = "/media/fat/_Utility/$($profileConfig.remotePrefix)-$($localHash.Substring(0, 12)).rbf"
        $receipt.localRbf = [ordered]@{ path = $localRbfPath; bytes = $localBytes; sha256 = $localHash }
        $receipt.remoteRbf = $remotePath

        $exists = @(Invoke-BoardSsh "if test -e $remotePath; then echo present; else echo absent; fi")[-1]
        if ($exists -ne 'absent') {
            throw "Remote staging path already exists; refusing to overwrite it: $remotePath"
        }

        $createdRemoteFile = $true
        & scp.exe @scpOptions $localRbfPath "${target}:$remotePath"
        if ($LASTEXITCODE -ne 0) { throw "SCP failed with exit code $LASTEXITCODE" }

        $remoteHash = @(Invoke-BoardSsh "sync; sha256sum $remotePath | cut -d' ' -f1")[-1]
        if ($remoteHash -ne $localHash) {
            throw "Remote RBF hash mismatch: expected $localHash, got $remoteHash"
        }

        $watchdogDelay = if ($ExerciseWatchdog) { $HoldSeconds } else { $HoldSeconds + 15 }
        $watchdog = Arm-BoardRollbackWatchdog `
            -Identifier $localHash.Substring(0, 12) `
            -DelaySeconds $watchdogDelay
        $receipt.watchdog = $watchdog

        $probeLoadAttempted = $true
        Set-BoardCore $remotePath
        Start-Sleep -Seconds 4
        $receipt.loaded = @(Get-BoardState)
        $receipt.loadedState = Assert-BoardState $receipt.loaded $profileConfig.expectedCore 'loaded probe'

        if ($ExerciseWatchdog) {
            Start-Sleep -Seconds ($HoldSeconds + 4)
            $receipt.rollbackAttempted = $true
            $receipt.afterRollback = @(Get-BoardState)
            $receipt.afterRollbackState = Assert-BoardState $receipt.afterRollback 'MENU' 'HPS watchdog rollback'
            $watchdogRollbackObserved = $true
            $receipt.rollbackSucceeded = $true
            $receipt.status = 'watchdog-rollback-observed'
        } else {
            Start-Sleep -Seconds $HoldSeconds
            $receipt.status = 'probe-observed'
        }
    }
} catch {
    $pendingError = $_
    $receipt.error = $_.Exception.Message
    $receipt.status = 'failed'
} finally {
    if ($probeLoadAttempted -and -not $watchdogRollbackObserved) {
        $receipt.rollbackAttempted = $true
        try {
            Set-BoardCore $menuPath
            Start-Sleep -Seconds 4
            $receipt.afterRollback = @(Get-BoardState)
            $receipt.afterRollbackState = Assert-BoardState $receipt.afterRollback 'MENU' 'host rollback'
            $receipt.rollbackSucceeded = $true
            if ($receipt.status -eq 'probe-observed') { $receipt.status = 'ok' }
        } catch {
            $receipt.rollbackSucceeded = $false
            $receipt.status = 'rollback-failed'
            if ($null -eq $pendingError) { $pendingError = $_ }
            $receipt.error = $_.Exception.Message
        }
    }

    if ($null -ne $watchdog -and $receipt.rollbackSucceeded) {
        try {
            $receipt.watchdogLog = @(Disarm-BoardRollbackWatchdog $watchdog)
            if ($ExerciseWatchdog) {
                $receipt.watchdogEvidence = Assert-WatchdogEvidence $receipt.watchdogLog
            } elseif ($receipt.watchdogLog.Count -ne 0) {
                throw "Disarmed watchdog emitted unexpected evidence: $($receipt.watchdogLog -join '; ')"
            }
            if ($receipt.status -eq 'watchdog-rollback-observed') { $receipt.status = 'ok' }
        } catch {
            if ($null -eq $pendingError) { $pendingError = $_ }
            $receipt.status = 'watchdog-cleanup-failed'
            $receipt.error = $_.Exception.Message
        }
    }

    if ($createdRemoteFile -and ($receipt.rollbackSucceeded -or -not $probeLoadAttempted) -and $remotePath) {
        try {
            Invoke-BoardSsh "rm -f $remotePath && sync" | Out-Null
            $receipt.stagedFileRemoved = $true
        } catch {
            if ($null -eq $pendingError) { $pendingError = $_ }
            $receipt.status = 'cleanup-failed'
            $receipt.error = $_.Exception.Message
        }
    }

    $receipt.completedUtc = (Get-Date).ToUniversalTime().ToString('o')
    if (-not $ReceiptPath) {
        $runDir = Join-Path $repoRoot 'runs\CLAUDE-RUNS\RUN-20260913-1651-board-bringup'
        $leaf = if ($IdentityPreflight) { 'IDENTITY-PREFLIGHT.json' } elseif ($RehearseRollback) { 'ROLLBACK-REHEARSAL.json' } elseif ($ExerciseWatchdog) { 'WATCHDOG-FIRE-TEST.json' } else { $profileConfig.receipt }
        $ReceiptPath = Join-Path $runDir $leaf
    }
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText(
        [System.IO.Path]::GetFullPath($ReceiptPath),
        ($receipt | ConvertTo-Json -Depth 8) + "`n",
        $utf8NoBom
    )

    Restore-EnvironmentValue 'SSH_ASKPASS' $oldAskPass
    Restore-EnvironmentValue 'SSH_ASKPASS_REQUIRE' $oldAskPassRequire
    Restore-EnvironmentValue 'DISPLAY' $oldDisplay
    if ($knownHostsFile -and (Test-Path -LiteralPath $knownHostsFile -PathType Leaf)) {
        Remove-Item -LiteralPath $knownHostsFile -Force -Confirm:$false
    }
}

if ($null -ne $pendingError) {
    throw $pendingError
}

$receipt | ConvertTo-Json -Depth 8
