[CmdletBinding(DefaultParameterSetName = 'Probe')]
param(
    [Parameter(Mandatory = $true, ParameterSetName = 'Probe')]
    [string]$RbfPath,

    [Parameter(Mandatory = $true, ParameterSetName = 'Rollback')]
    [switch]$RehearseRollback,

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
$target = "$UserName@$HostName"
$profileConfig = if ($Profile -eq 'Specs') {
    [ordered]@{
        project = 'ZhaozhouSpecs'
        expectedCore = 'Zhaozhou Hardware Specs'
        verifier = 'tools\board\verify_superstation_specs.py'
        audit = 'runs\CLAUDE-RUNS\RUN-20260913-1651-board-bringup\HARDWARE-SPECS-BUILD-AUDIT.json'
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
        audit = 'runs\CLAUDE-RUNS\RUN-20260913-1651-board-bringup\QUARTUS-BUILD-AUDIT.json'
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

if (-not $Execute) {
    throw 'This tool changes the live FPGA image. Pass -Execute only after reviewing the build and rollback receipt.'
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

$sshOptions = @(
    '-T',
    '-o', 'ConnectTimeout=8',
    '-o', 'ConnectionAttempts=1',
    '-o', 'StrictHostKeyChecking=yes',
    '-o', 'NumberOfPasswordPrompts=1'
)
$scpOptions = @(
    '-q',
    '-o', 'ConnectTimeout=8',
    '-o', 'ConnectionAttempts=1',
    '-o', 'StrictHostKeyChecking=yes',
    '-o', 'NumberOfPasswordPrompts=1'
)

function Invoke-BoardSsh {
    param([Parameter(Mandatory = $true)][string]$Command)
    $output = @(& ssh.exe @sshOptions $target $Command)
    if ($LASTEXITCODE -ne 0) {
        throw "SSH command failed with exit code ${LASTEXITCODE}: $Command"
    }
    return $output
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
    mode = if ($RehearseRollback) { 'rollback-rehearsal' } elseif ($ExerciseWatchdog) { 'watchdog-fire-test' } else { 'probe-transaction' }
    before = @()
    loaded = @()
    afterRollback = @()
    localRbf = $null
    remoteRbf = $null
    menuRbf = [ordered]@{ path = $menuPath; expectedSha256 = $menuSha256; observedSha256 = $null }
    watchdog = $null
    watchdogLog = @()
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
    $receipt.before = @(Get-BoardState)
    Invoke-BoardSsh "test -p /dev/MiSTer_cmd && test -r $menuPath && test `$(cat /sys/class/fpga_manager/fpga0/state) = operating" | Out-Null
    $observedMenuHash = @(Invoke-BoardSsh "sha256sum $menuPath | cut -d' ' -f1")[-1]
    $receipt.menuRbf.observedSha256 = $observedMenuHash
    if ($observedMenuHash -ne $menuSha256) {
        throw "Rollback menu hash mismatch: expected $menuSha256, got $observedMenuHash"
    }

    if ($RehearseRollback) {
        $receipt.rollbackAttempted = $true
        Set-BoardCore $menuPath
        Start-Sleep -Seconds 4
        $receipt.afterRollback = @(Get-BoardState)
        $coreLine = $receipt.afterRollback | Where-Object { $_ -like 'core=*' } | Select-Object -First 1
        if ($coreLine -notmatch '^core=MENU') {
            throw "Menu rollback rehearsal did not report MENU: $coreLine"
        }
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
        $loadedCore = $receipt.loaded | Where-Object { $_ -like 'core=*' } | Select-Object -First 1
        $loadedRbf = $receipt.loaded | Where-Object { $_ -like 'rbf=*' } | Select-Object -First 1
        $expectedCorePattern = '^core=' + [regex]::Escape($profileConfig.expectedCore)
        $expectedRbfPattern = '^rbf=' + [regex]::Escape($profileConfig.expectedCore)
        if ($loadedCore -notmatch $expectedCorePattern) {
            throw "Probe did not report its expected core name: $loadedCore"
        }
        if ($loadedRbf -notmatch $expectedRbfPattern) {
            throw "Probe did not report its expected RBF identity: $loadedRbf"
        }

        if ($ExerciseWatchdog) {
            Start-Sleep -Seconds ($HoldSeconds + 4)
            $receipt.rollbackAttempted = $true
            $receipt.afterRollback = @(Get-BoardState)
            $watchdogCore = $receipt.afterRollback | Where-Object { $_ -like 'core=*' } | Select-Object -First 1
            if ($watchdogCore -notmatch '^core=MENU') {
                throw "HPS watchdog did not return the board to MENU: $watchdogCore"
            }
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
            $coreLine = $receipt.afterRollback | Where-Object { $_ -like 'core=*' } | Select-Object -First 1
            if ($coreLine -notmatch '^core=MENU') {
                throw "Rollback did not report MENU: $coreLine"
            }
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
            if ($ExerciseWatchdog -and -not ($receipt.watchdogLog | Where-Object { $_ -like 'watchdog-fired=*' })) {
                throw 'Board returned to MENU but the HPS watchdog left no fire log.'
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
        $leaf = if ($RehearseRollback) { 'ROLLBACK-REHEARSAL.json' } elseif ($ExerciseWatchdog) { 'WATCHDOG-FIRE-TEST.json' } else { $profileConfig.receipt }
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
}

if ($null -ne $pendingError) {
    throw $pendingError
}

$receipt | ConvertTo-Json -Depth 8
