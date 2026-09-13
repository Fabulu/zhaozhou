[CmdletBinding()]
param(
    [Alias('ParityOnly')]
    [switch]$PreflightOnly,
    [switch]$KeepWorkspace,
    [switch]$TestOnlyFakeQuartus,
    [string]$PythonExe,
    [string]$QuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64',
    [string]$ReportRoot,
    [ValidateRange(1, 256)]
    [int]$Processors = 4
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$GateName = 'shell_fit_top_clean_characterization'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProjectRel = 'fpga/quartus/shell_fit'
$QsfRel = "$ProjectRel/zhao_shell_fit.qsf"
$SdcRel = "$ProjectRel/zhao_shell_fit.sdc"
$QpfRel = "$ProjectRel/zhao_shell_fit.qpf"
$ReportScriptRel = "$ProjectRel/report.tcl"
$PostMapScriptRel = "$ProjectRel/post_map_connectivity.tcl"
$CmakeRel = 'tests/CMakeLists.txt'
$WrapperRel = 'fpga/rtl/generated/zhao_shell_fit_top.sv'
$ManifestRel = 'fpga/rtl/generated/zhao_shell_fit_top.manifest.json'
$LauncherRel = 'tools/quartus/run_shell_fit.ps1'
$CanonicalQuartusBin = 'C:\intelFPGA_lite\17.0\quartus\bin64'
$CanonicalQuartusSha256 = @{
    'quartus_map.exe' = '3DFB729AF88E2EF6217EC6EA0DBAD305485814B84B46610A1CFD41EFF5DBCD45'
    'quartus_fit.exe' = 'D7C802E80E332AEF0D5D711077B033FE8BEB9E58A2302A3B3F2C5C8CCD7A0917'
    'quartus_sta.exe' = '5CD28F77B246F2B1324D76E01D15575FD5AB8E9B9297284126701278B8E325A9'
}
$CanonicalPythonExePath = 'C:\Users\Fabs\AppData\Local\Programs\Python\Python312\python.exe'
$CanonicalPythonExeSha256 = '4d6f5f81a4bca11191c4c7c6b43632694d0a4ce74e068619d8fdc161d469859a'
$CanonicalPythonExeSize = 104952L
$CanonicalPython312DllSha256 = '9a0e3435aaa680d868150f87ab3e388ad2eebc22f87e036155c7b4eda8cd2120'
$CanonicalPython312DllSize = 6945272L
$CanonicalPython3DllSha256 = 'fb975a606e7fbf74f64260e3f60c3490b4f74a183c0926fd6ed1ac4c52ac7b1c'
$CanonicalPython3DllSize = 70376L
$CanonicalPythonSignerThumbprint = 'DE01DAAE82D04F466A576E178F6B07A839238953'
$CanonicalPythonSignerSubject = 'CN=Python Software Foundation, O=Python Software Foundation, L=Beaverton, S=Oregon, C=US'
$CanonicalPythonTimestampThumbprint = 'AC3199ABB3D05D51499E1A5342738297D8110FA9'
$CanonicalPythonTimestampSubject = 'CN=Microsoft Public RSA Time Stamping Authority, OU=Thales TSS ESN:3DA5-963B-E1F4, OU=Microsoft America Operations, O=Microsoft Corporation, L=Redmond, S=Washington, C=US'
$CanonicalPythonTimestampIssuer = 'CN=Microsoft Public RSA Timestamping CA 2020, O=Microsoft Corporation, C=US'

function Assert-PinnedPythonFile(
    [string]$Path,
    [string]$Label,
    [long]$ExpectedSize,
    [string]$ExpectedSha256
) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Pinned CPython $Label not found: $Path"
    }
    $resolved = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Path).Path)
    $expected = [IO.Path]::GetFullPath($Path)
    if (-not [string]::Equals($resolved, $expected, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Pinned CPython $Label resolves outside its canonical location: '$resolved'."
    }
    $actualSize = (Get-Item -LiteralPath $resolved).Length
    if ($actualSize -ne $ExpectedSize) {
        throw "Pinned CPython $Label size mismatch: expected $ExpectedSize, got $actualSize."
    }
    $actualSha256 = (Get-FileHash -LiteralPath $resolved -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actualSha256 -cne $ExpectedSha256) {
        throw "Pinned CPython $Label SHA-256 mismatch: $actualSha256"
    }
}

function Assert-PinnedPythonInstallation([string]$RequestedFull, [string]$Actual) {
    $canonical = [IO.Path]::GetFullPath($CanonicalPythonExePath)
    if (
        -not [string]::Equals($RequestedFull, $canonical, [StringComparison]::OrdinalIgnoreCase) -or
        -not [string]::Equals($Actual, $canonical, [StringComparison]::OrdinalIgnoreCase)
    ) {
        throw "PythonExe must name the pinned CPython installation '$canonical'; caller resolved '$Actual'."
    }

    $installRoot = Split-Path -Parent $canonical
    Assert-PinnedPythonFile $canonical 'python.exe' $CanonicalPythonExeSize $CanonicalPythonExeSha256
    Assert-PinnedPythonFile (Join-Path $installRoot 'python312.dll') 'python312.dll' $CanonicalPython312DllSize $CanonicalPython312DllSha256
    Assert-PinnedPythonFile (Join-Path $installRoot 'python3.dll') 'python3.dll' $CanonicalPython3DllSize $CanonicalPython3DllSha256

    $signature = Get-AuthenticodeSignature -FilePath $canonical
    $signatureStatus = $signature.Status.ToString()
    $signer = $signature.SignerCertificate
    $timestamp = $signature.TimeStamperCertificate
    if ($signatureStatus -cne 'Valid') {
        throw "Pinned CPython python.exe Authenticode status is '$signatureStatus', not Valid: '$canonical'."
    }
    if ($null -eq $signer) {
        throw "Pinned CPython python.exe has no Authenticode signer certificate: '$canonical'."
    }
    if ($null -eq $timestamp) {
        throw "Pinned CPython python.exe has no Authenticode timestamp certificate: '$canonical'."
    }

    $signerThumbprint = ($signer.Thumbprint -replace '\s', '').ToUpperInvariant()
    $signerSubject = $signer.Subject
    if (
        $signerThumbprint -cne $CanonicalPythonSignerThumbprint -or
        $signerSubject -cne $CanonicalPythonSignerSubject
    ) {
        throw "Pinned CPython Authenticode signer mismatch: thumbprint '$signerThumbprint', subject '$signerSubject'."
    }

    $timestampThumbprint = ($timestamp.Thumbprint -replace '\s', '').ToUpperInvariant()
    $timestampSubject = $timestamp.Subject
    $timestampIssuer = $timestamp.Issuer
    if (
        $timestampThumbprint -cne $CanonicalPythonTimestampThumbprint -or
        $timestampSubject -cne $CanonicalPythonTimestampSubject -or
        $timestampIssuer -cne $CanonicalPythonTimestampIssuer
    ) {
        throw "Pinned CPython Authenticode timestamp mismatch: thumbprint '$timestampThumbprint', subject '$timestampSubject', issuer '$timestampIssuer'."
    }
}

function ConvertTo-WindowsNativeArgument([string]$Argument) {
    $quoted = [Text.StringBuilder]::new()
    [void]$quoted.Append('"')
    $backslashes = 0
    foreach ($character in $Argument.ToCharArray()) {
        if ($character -eq [char]92) {
            $backslashes += 1
            continue
        }
        if ($character -eq [char]34) {
            if ($backslashes -gt 0) {
                [void]$quoted.Append((('\' * (2 * $backslashes + 1)) -join ''))
            } else {
                [void]$quoted.Append('\')
            }
            [void]$quoted.Append('"')
            $backslashes = 0
            continue
        }
        if ($backslashes -gt 0) {
            [void]$quoted.Append((('\' * $backslashes) -join ''))
            $backslashes = 0
        }
        [void]$quoted.Append($character)
    }
    if ($backslashes -gt 0) {
        [void]$quoted.Append((('\' * (2 * $backslashes)) -join ''))
    }
    [void]$quoted.Append('"')
    return $quoted.ToString()
}

function Invoke-PythonIdentityProbe(
    [string]$Executable,
    [string]$ProbeScript,
    [string]$Nonce
) {
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $replacementUtf8 = [Text.UTF8Encoding]::new($false, $false)
    $startInfo.StandardOutputEncoding = $replacementUtf8
    $startInfo.StandardErrorEncoding = $replacementUtf8

    $arguments = @('-I', '-c', $ProbeScript, $Nonce)
    $argumentList = $startInfo.PSObject.Properties['ArgumentList']
    if ($null -ne $argumentList) {
        foreach ($argument in $arguments) {
            [void]$startInfo.ArgumentList.Add($argument)
        }
    } else {
        $startInfo.Arguments = (@(
            $arguments | ForEach-Object { ConvertTo-WindowsNativeArgument $_ }
        ) -join ' ')
    }

    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    try {
        if (-not $process.Start()) {
            throw 'native process start returned false'
        }
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            Stdout = $stdout
            Stderr = $stderr
        }
    } catch {
        throw "PythonExe could not be executed as a real Python interpreter: '$Executable' ($($_.Exception.Message))"
    } finally {
        $process.Dispose()
    }
}

function Resolve-PythonExecutable([string]$RequestedExecutable) {
    if ([string]::IsNullOrWhiteSpace($RequestedExecutable)) {
        $RequestedExecutable = $CanonicalPythonExePath
    }
    if (-not [IO.Path]::IsPathRooted($RequestedExecutable)) {
        throw "PythonExe must be an absolute path; bare or PATH-resolved interpreters are forbidden: '$RequestedExecutable'."
    }

    $requestedFull = [IO.Path]::GetFullPath($RequestedExecutable)
    if (@($requestedFull -split '[\\/]' | Where-Object { $_ -ieq 'WindowsApps' }).Count -ne 0) {
        throw "PythonExe must not be a WindowsApps/app-execution alias: '$requestedFull'."
    }
    if (-not (Test-Path -LiteralPath $requestedFull -PathType Leaf)) {
        throw "Compatible Python 3.12 executable not found: $requestedFull"
    }

    $actual = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $requestedFull).Path)
    if (@($actual -split '[\\/]' | Where-Object { $_ -ieq 'WindowsApps' }).Count -ne 0) {
        throw "PythonExe resolves to a forbidden WindowsApps/app-execution alias: '$actual'."
    }

    Assert-PinnedPythonInstallation $requestedFull $actual

    $nonce = [guid]::NewGuid().ToString('N')
    $probeScript = 'import base64,sys; print("ZHAO_SHELL_FIT_PYTHON_V1|{}|{}|{}|{}|{}|{}".format(sys.argv[1],sys.implementation.name,sys.version_info.major,sys.version_info.minor,sys.version_info.micro,base64.b64encode(sys.executable.encode("utf-8")).decode("ascii")))'
    $probe = Invoke-PythonIdentityProbe $actual $probeScript $nonce
    $probeExitCode = $probe.ExitCode
    $probeStdout = [string]$probe.Stdout
    $probeStderr = [string]$probe.Stderr
    if ($probeExitCode -ne 0) {
        throw "PythonExe probe failed with exit code $probeExitCode`: '$actual' (stdout='$probeStdout'; stderr='$probeStderr')"
    }
    if (-not [string]::IsNullOrEmpty($probeStderr)) {
        throw "PythonExe probe produced unexpected stderr: '$actual' ('$probeStderr')."
    }
    if ($probeStdout.EndsWith("`r`n", [StringComparison]::Ordinal)) {
        $probeLine = $probeStdout.Substring(0, $probeStdout.Length - 2)
    } elseif ($probeStdout.EndsWith("`n", [StringComparison]::Ordinal)) {
        $probeLine = $probeStdout.Substring(0, $probeStdout.Length - 1)
    } else {
        throw "PythonExe probe produced unterminated or empty output: '$actual'."
    }
    if ([string]::IsNullOrEmpty($probeLine) -or $probeLine.IndexOfAny([char[]]"`r`n") -ne -1) {
        throw "PythonExe probe produced unexpected output: '$actual'."
    }
    $probeRows = @($probeLine)
    if ($probeRows.Count -ne 1) {
        throw "PythonExe probe produced unexpected output: '$actual'."
    }

    $fields = ([string]$probeRows[0]) -split '\|', 7
    if ($fields.Count -ne 7 -or $fields[0] -cne 'ZHAO_SHELL_FIT_PYTHON_V1' -or $fields[1] -cne $nonce) {
        throw "PythonExe probe output did not authenticate the requested interpreter: '$actual'."
    }
    if ($fields[2] -cne 'cpython' -or $fields[3] -cne '3' -or $fields[4] -cne '12' -or $fields[5] -notmatch '^\d+$') {
        throw "PythonExe must report CPython 3.12; probe reported '$($fields[2]) $($fields[3]).$($fields[4]).$($fields[5])' from '$actual'."
    }
    try {
        $reportedExecutable = [Text.Encoding]::UTF8.GetString(
            [Convert]::FromBase64String($fields[6])
        )
        $reportedFull = [IO.Path]::GetFullPath($reportedExecutable)
    } catch {
        throw "PythonExe probe reported an invalid executable identity: '$actual'."
    }
    if (-not [string]::Equals($reportedFull, $actual, [StringComparison]::OrdinalIgnoreCase)) {
        throw "PythonExe probe identity mismatch: requested '$actual', running '$reportedFull'."
    }

    $versionInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($actual)
    if (
        -not [string]::Equals([IO.Path]::GetExtension($actual), '.exe', [StringComparison]::OrdinalIgnoreCase) -or
        -not [string]::Equals([IO.Path]::GetFileName($actual), 'python.exe', [StringComparison]::OrdinalIgnoreCase) -or
        -not [string]::Equals($versionInfo.OriginalFilename, 'python.exe', [StringComparison]::OrdinalIgnoreCase) -or
        -not [string]::Equals($versionInfo.ProductName, 'Python', [StringComparison]::OrdinalIgnoreCase) -or
        -not [string]::Equals($versionInfo.CompanyName, 'Python Software Foundation', [StringComparison]::OrdinalIgnoreCase) -or
        $versionInfo.FileMajorPart -ne 3 -or
        $versionInfo.FileMinorPart -ne 12
    ) {
        throw "PythonExe is not a genuine CPython 3.12 python.exe: '$actual'."
    }
    return $actual
}

$Python = Resolve-PythonExecutable $PythonExe

function Invoke-Checked([string]$Executable, [string[]]$Arguments, [string]$Label) {
    & $Executable @Arguments
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "$Label failed with exit code $exitCode."
    }
}

function Get-RepoRelativePath([string]$AbsolutePath, [string]$Root) {
    $absolute = [IO.Path]::GetFullPath($AbsolutePath).TrimEnd('\')
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\')
    if (-not $absolute.StartsWith($rootFull + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path '$absolute' escapes repository root '$rootFull'."
    }
    return $absolute.Substring($rootFull.Length + 1).Replace('\', '/')
}

function Assert-CommittedFlowFile([string]$RelativePath, [string]$Commit) {
    & git -C $RepoRoot --no-replace-objects cat-file -e "$Commit`:$RelativePath" 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "Real characterization requires committed flow/source-cone path: $RelativePath"
    }
}

function Assert-LiveLauncherEquivalent([string]$Commit) {
    & $Python (Join-Path $RepoRoot 'tools\quartus\capture_shell_fit_git.py') `
        --repo-root $RepoRoot `
        --compare-crlf-only (Join-Path $RepoRoot ($LauncherRel -replace '/', '\')) `
        --commit $Commit
    if ($LASTEXITCODE -ne 0) {
        throw "The shell-fit launcher differs from the captured commit beyond CRLF/LF normalization."
    }
}

function Assert-TrackedTreeClean([string]$Commit) {
    $statusRows = @(& git -C $RepoRoot --no-replace-objects status --short --untracked-files=no)
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not inspect whole tracked worktree status.'
    }
    if ($statusRows.Count -ne 0) {
        throw "Whole tracked tree is dirty at shell-fit invocation: $($statusRows -join '; ')"
    }
    & git -C $RepoRoot --no-replace-objects diff --quiet --no-ext-diff --no-textconv --binary --
    if ($LASTEXITCODE -ne 0) {
        throw 'Whole tracked worktree is dirty at shell-fit invocation.'
    }
    & git -C $RepoRoot --no-replace-objects diff --cached --quiet --no-ext-diff --no-textconv --binary --
    if ($LASTEXITCODE -ne 0) {
        throw 'Whole tracked index is dirty at shell-fit invocation.'
    }
    $flagRows = @(& git -C $RepoRoot --no-replace-objects ls-files --cached -v)
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not inspect whole-tree index flags.'
    }
    $concealed = @($flagRows | Where-Object { $_ -and $_[0] -cne 'H' })
    if ($concealed.Count -ne 0) {
        throw "Tracked paths use index concealment flags: $($concealed -join '; ')"
    }
    Assert-LiveLauncherEquivalent $Commit
}

function Resolve-QuartusExecutables([string]$RequestedBin, [bool]$TestOnly) {
    $names = @('quartus_map.exe', 'quartus_fit.exe', 'quartus_sta.exe')
    $requestedFull = [IO.Path]::GetFullPath($RequestedBin).TrimEnd('\')
    $canonicalFull = [IO.Path]::GetFullPath($CanonicalQuartusBin).TrimEnd('\')
    if (-not $TestOnly -and -not [string]::Equals($requestedFull, $canonicalFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Production shell fit requires canonical Quartus bin '$canonicalFull'; caller supplied '$requestedFull'."
    }
    $resolved = [ordered]@{}
    foreach ($name in $names) {
        $expected = Join-Path $requestedFull $name
        if (-not (Test-Path -LiteralPath $expected -PathType Leaf)) {
            throw "Required Quartus executable not found: $expected"
        }
        $actual = (Resolve-Path -LiteralPath $expected).Path
        if (-not $TestOnly) {
            $canonicalExecutable = [IO.Path]::GetFullPath(
                (Join-Path $canonicalFull $name)
            )
            if (-not [string]::Equals($actual, $canonicalExecutable, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Production Quartus executable resolves outside canonical install: $actual"
            }
            $actualHash = (Get-FileHash -LiteralPath $actual -Algorithm SHA256).Hash
            if ($actualHash -cne $CanonicalQuartusSha256[$name]) {
                throw "Production Quartus binary identity mismatch for $name`: $actualHash"
            }
        }
        $resolved[$name] = $actual
    }
    return $resolved
}

function Invoke-QsfPreflight([string]$Root, [string]$EmitModel = '') {
    $arguments = @(
        (Join-Path $Root 'tools\quartus\shell_fit_qsf.py'),
        '--repo-root', $Root,
        '--cmake', (Join-Path $Root ($CmakeRel -replace '/', '\')),
        '--qsf', (Join-Path $Root ($QsfRel -replace '/', '\')),
        '--qpf', (Join-Path $Root ($QpfRel -replace '/', '\')),
        '--sdc', (Join-Path $Root ($SdcRel -replace '/', '\')),
        '--wrapper', $WrapperRel,
        '--top', 'zhao_shell_fit_top'
    )
    if (-not [string]::IsNullOrWhiteSpace($EmitModel)) {
        $arguments += @('--emit-model', $EmitModel)
    }
    Invoke-Checked $Python $arguments 'shell-fit QPF/QSF/SDC/source preflight'
}

function Assert-WrapperFreshness([string]$Root) {
    Push-Location $Root
    try {
        Invoke-Checked $Python @('tools/quartus/gen_shell_fit_top.py', '--check') 'generated wrapper freshness'
    } finally {
        Pop-Location
    }
}

function Publish-FileAtomic([string]$Source, [string]$Destination) {
    $directory = Split-Path -Parent $Destination
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $temporary = Join-Path $directory ('.{0}.{1}.tmp' -f ([IO.Path]::GetFileName($Destination)), [guid]::NewGuid().ToString('N'))
    try {
        $bytes = [IO.File]::ReadAllBytes($Source)
        $stream = [IO.FileStream]::new(
            $temporary,
            [IO.FileMode]::CreateNew,
            [IO.FileAccess]::Write,
            [IO.FileShare]::None,
            4096,
            [IO.FileOptions]::WriteThrough
        )
        try {
            $stream.Write($bytes, 0, $bytes.Length)
            $stream.Flush($true)
        } finally {
            $stream.Dispose()
        }
        if (Test-Path -LiteralPath $Destination -PathType Leaf) {
            [IO.File]::Replace($temporary, $Destination, $null)
        } else {
            [IO.File]::Move($temporary, $Destination)
        }
    } finally {
        if (Test-Path -LiteralPath $temporary) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}

function Protect-SnapshotFiles([string]$Root) {
    $count = 0
    foreach ($path in [IO.Directory]::EnumerateFiles(
        $Root,
        '*',
        [IO.SearchOption]::AllDirectories
    )) {
        $file = [IO.FileInfo]::new($path)
        $file.IsReadOnly = $true
        if (-not $file.IsReadOnly) {
            throw "Could not make frozen snapshot file read-only: $path"
        }
        $count++
    }
    if ($count -eq 0) {
        throw 'Frozen snapshot contains no files to protect.'
    }
    return $count
}

if ($PreflightOnly) {
    Invoke-QsfPreflight $RepoRoot
    Assert-WrapperFreshness $RepoRoot
    Write-Host "PASS $GateName preflight (Quartus not invoked)."
    exit 0
}

$mutex = [Threading.Mutex]::new($false, 'Global\ZhaoShellFitQuartusCharacterization')
$mutexHeld = $false
$Workspace = $null
$ArchiveZip = $null
$oldGitDir = $env:GIT_DIR
$oldGitWorkTree = $env:GIT_WORK_TREE
$oldGitIndex = $env:GIT_INDEX_FILE
$scriptExitCode = 0
try {
    try {
        $mutexHeld = $mutex.WaitOne(0)
    } catch [Threading.AbandonedMutexException] {
        $mutexHeld = $true
    }
    if (-not $mutexHeld) {
        throw "Another machine-wide shell-fit characterization holds the $GateName lock."
    }

    $head = (& git -C $RepoRoot --no-replace-objects rev-parse --verify 'HEAD^{commit}').Trim()
    if ($LASTEXITCODE -ne 0 -or $head -notmatch '^[0-9a-f]{40}$') {
        throw 'Could not resolve an exact repository HEAD commit.'
    }

    $FlowFiles = @(
        $QpfRel,
        $QsfRel,
        $SdcRel,
        $ReportScriptRel,
        $PostMapScriptRel,
        $LauncherRel,
        'tools/quartus/capture_shell_fit_git.py',
        'tools/quartus/shell_fit_qsf.py',
        'tools/quartus/shell_fit_reports.py',
        'tools/quartus/shell_ports.py',
        'tools/quartus/gen_shell_fit_top.py',
        $CmakeRel,
        $WrapperRel,
        $ManifestRel,
        'design/shell_fit_ports.yml',
        'tests/tools/fixtures/shell_fit_frame_blit.bin'
    )
    foreach ($path in $FlowFiles) {
        Assert-CommittedFlowFile $path $head
    }
    $LiveModel = Join-Path $RepoRoot ("reports\.shell-fit-model-{0}.json" -f [guid]::NewGuid().ToString('N'))
    try {
        Invoke-QsfPreflight $RepoRoot $LiveModel
        $model = Get-Content -LiteralPath $LiveModel -Raw | ConvertFrom-Json
        $sourceCone = @($model.sources)
        $constraintCone = @($model.constraints | ForEach-Object { $_.path })
    } finally {
        if (Test-Path -LiteralPath $LiveModel) {
            Remove-Item -LiteralPath $LiveModel -Force
        }
    }
    foreach ($path in @($sourceCone + $constraintCone)) {
        Assert-CommittedFlowFile $path $head
    }
    Assert-TrackedTreeClean $head

    $QuartusExecutables = Resolve-QuartusExecutables $QuartusBin $TestOnlyFakeQuartus.IsPresent
    $QuartusMap = $QuartusExecutables['quartus_map.exe']
    $QuartusFit = $QuartusExecutables['quartus_fit.exe']
    $QuartusSta = $QuartusExecutables['quartus_sta.exe']

    if ([string]::IsNullOrWhiteSpace($ReportRoot)) {
        $ReportRoot = Join-Path $RepoRoot 'reports'
    } elseif (-not [IO.Path]::IsPathRooted($ReportRoot)) {
        $ReportRoot = Join-Path $RepoRoot $ReportRoot
    }
    $ReportRoot = [IO.Path]::GetFullPath($ReportRoot)
    $ReportRootRel = Get-RepoRelativePath $ReportRoot $RepoRoot

    $runStamp = (Get-Date).ToUniversalTime().ToString('yyyyMMddTHHmmssZ')
    $runId = "$($head.Substring(0, 12))-$runStamp-$PID"
    $Workspace = Join-Path ([IO.Path]::GetTempPath()) ("zhao-shell-fit-{0}-{1}" -f $PID, [guid]::NewGuid().ToString('N'))
    $ArchiveZip = "$Workspace.zip"
    $Snapshot = Join-Path $Workspace 'source'
    $FrozenGit = Join-Path $Workspace 'repository.git'
    $PrivateIndex = Join-Path $Workspace 'git-index'
    New-Item -ItemType Directory -Path $Snapshot -Force | Out-Null

    Invoke-Checked git @(
        '-C', $RepoRoot, '-c', 'core.autocrlf=false', '--no-replace-objects',
        'archive', '--format=zip', "--output=$ArchiveZip", $head
    ) 'git archive captured raw commit blobs'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::ExtractToDirectory($ArchiveZip, $Snapshot)
    Remove-Item -LiteralPath $ArchiveZip -Force
    $ArchiveZip = $null

    Invoke-Checked git @('clone', '--bare', '--shared', $RepoRoot, $FrozenGit) 'frozen Git object view'
    Invoke-Checked git @("--git-dir=$FrozenGit", '--no-replace-objects', 'update-ref', 'refs/heads/evidence', $head) 'freeze evidence ref'
    Invoke-Checked git @("--git-dir=$FrozenGit", '--no-replace-objects', 'symbolic-ref', 'HEAD', 'refs/heads/evidence') 'freeze evidence HEAD'

    $env:GIT_DIR = $FrozenGit
    $env:GIT_WORK_TREE = $Snapshot
    $env:GIT_INDEX_FILE = $PrivateIndex
    Invoke-Checked git @('-C', $Snapshot, '--no-replace-objects', 'read-tree', $head) 'private-index read-tree'

    Invoke-QsfPreflight $Snapshot
    Assert-WrapperFreshness $Snapshot

    $SnapshotProject = Join-Path $Snapshot ($ProjectRel -replace '/', '\')
    $SnapshotReportRoot = Join-Path $Snapshot ($ReportRootRel -replace '/', '\')
    $RunRoot = Join-Path $SnapshotReportRoot "characterization\$GateName\$runId"
    $RawQuartus = Join-Path $RunRoot 'raw\quartus'
    $RawGit = Join-Path $RunRoot 'raw\git'
    $StageLogs = Join-Path $RunRoot 'stage-logs'
    New-Item -ItemType Directory -Path $RawQuartus -Force | Out-Null
    New-Item -ItemType Directory -Path $RawGit -Force | Out-Null
    New-Item -ItemType Directory -Path $StageLogs -Force | Out-Null

    Invoke-Checked $Python @(
        (Join-Path $Snapshot 'tools\quartus\capture_shell_fit_git.py'),
        '--repo-root', $Snapshot,
        '--output-dir', $RawGit
    ) 'clean frozen Git evidence capture'
    $protectedFiles = Protect-SnapshotFiles $Snapshot
    Write-Host "shell-fit-snapshot: protected $protectedFiles frozen files as read-only"

    function Invoke-QuartusStage([string]$Name, [string]$Executable, [string[]]$Arguments) {
        $stdoutPath = Join-Path $StageLogs "$Name.stdout.log"
        $stderrPath = Join-Path $StageLogs "$Name.stderr.log"
        Write-Host "RUN $Name`: $([IO.Path]::GetFileName($Executable)) $($Arguments -join ' ')"
        $process = Start-Process -FilePath $Executable `
            -ArgumentList $Arguments `
            -WorkingDirectory $SnapshotProject `
            -NoNewWindow -Wait -PassThru `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath
        $exitCode = $process.ExitCode
        if (Test-Path -LiteralPath $stdoutPath) {
            Get-Content -LiteralPath $stdoutPath | ForEach-Object { Write-Host $_ }
        }
        if (Test-Path -LiteralPath $stderrPath) {
            Get-Content -LiteralPath $stderrPath | ForEach-Object { Write-Host $_ }
        }
        if ($exitCode -ne 0) {
            throw "Quartus stage '$Name' failed with exit code $exitCode; logs: $StageLogs"
        }
    }

    Invoke-QuartusStage 'map' $QuartusMap @('zhao_shell_fit', "--parallel=$Processors")
    Invoke-QuartusStage 'post-map' $QuartusSta @('zhao_shell_fit', '--post_map', '--report_script=post_map_connectivity.tcl')
    Invoke-QuartusStage 'fit' $QuartusFit @('zhao_shell_fit', "--parallel=$Processors")
    Invoke-QuartusStage 'timequest' $QuartusSta @('zhao_shell_fit', '--report_script=report.tcl')

    $OutputDir = Join-Path $SnapshotProject 'output_files'
    $CharacterizationDir = Join-Path $OutputDir 'characterization'
    $QuartusArtifacts = [ordered]@{
        'zhao_shell_fit.fit.summary' = Join-Path $OutputDir 'zhao_shell_fit.fit.summary'
        'zhao_shell_fit.fit.rpt' = Join-Path $OutputDir 'zhao_shell_fit.fit.rpt'
        'zhao_shell_fit.map.summary' = Join-Path $OutputDir 'zhao_shell_fit.map.summary'
        'zhao_shell_fit.map.rpt' = Join-Path $OutputDir 'zhao_shell_fit.map.rpt'
        'zhao_shell_fit.sta.rpt' = Join-Path $OutputDir 'zhao_shell_fit.sta.rpt'
        'timing_metrics.tsv' = Join-Path $CharacterizationDir 'timing_metrics.tsv'
        'clocks.rpt' = Join-Path $CharacterizationDir 'clocks.rpt'
        'clock_transfers.rpt' = Join-Path $CharacterizationDir 'clock_transfers.rpt'
        'unconstrained_paths.rpt' = Join-Path $CharacterizationDir 'unconstrained_paths.rpt'
        'setup_paths.rpt' = Join-Path $CharacterizationDir 'setup_paths.rpt'
        'hold_paths.rpt' = Join-Path $CharacterizationDir 'hold_paths.rpt'
        'recovery_paths.rpt' = Join-Path $CharacterizationDir 'recovery_paths.rpt'
        'removal_paths.rpt' = Join-Path $CharacterizationDir 'removal_paths.rpt'
        'post_map_connectivity.tsv' = Join-Path $CharacterizationDir 'post_map_connectivity.tsv'
    }
    foreach ($entry in $QuartusArtifacts.GetEnumerator()) {
        if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
            throw "Expected raw Quartus artifact missing: $($entry.Value)"
        }
        [IO.File]::Copy($entry.Value, (Join-Path $RawQuartus $entry.Key), $false)
    }

    $ReceiptPath = Join-Path $RunRoot 'receipt.json'
    $EvidenceMode = if ($TestOnlyFakeQuartus) { 'test-only' } else { 'production' }
    $ParserArgs = @(
        (Join-Path $Snapshot 'tools\quartus\shell_fit_reports.py'),
        '--repo-root', $Snapshot,
        '--summary', (Join-Path $RawQuartus 'zhao_shell_fit.fit.summary'),
        '--sta', (Join-Path $RawQuartus 'zhao_shell_fit.sta.rpt'),
        '--clocks', (Join-Path $RawQuartus 'clocks.rpt'),
        '--hierarchy', (Join-Path $RawQuartus 'zhao_shell_fit.fit.rpt'),
        '--map-summary', (Join-Path $RawQuartus 'zhao_shell_fit.map.summary'),
        '--map-report', (Join-Path $RawQuartus 'zhao_shell_fit.map.rpt'),
        '--emit-receipt', $ReceiptPath,
        '--manifest', (Join-Path $Snapshot ($ManifestRel -replace '/', '\')),
        '--rtl', (Join-Path $Snapshot ($WrapperRel -replace '/', '\')),
        '--shell', (Join-Path $Snapshot 'fpga\rtl\common\zhao_shell_top.sv'),
        '--package', (Join-Path $Snapshot 'fpga\rtl\common\zhao_pkg.sv'),
        '--policy', (Join-Path $Snapshot 'design\shell_fit_ports.yml'),
        '--generator', (Join-Path $Snapshot 'tools\quartus\gen_shell_fit_top.py'),
        '--parser', (Join-Path $Snapshot 'tools\quartus\shell_ports.py'),
        '--packet', (Join-Path $Snapshot 'tests\tools\fixtures\shell_fit_frame_blit.bin'),
        '--cmake', (Join-Path $Snapshot ($CmakeRel -replace '/', '\')),
        '--qsf', (Join-Path $Snapshot ($QsfRel -replace '/', '\')),
        '--sdc', (Join-Path $Snapshot ($SdcRel -replace '/', '\')),
        '--qsf-parser', (Join-Path $Snapshot 'tools\quartus\shell_fit_qsf.py'),
        '--evidence-parser', (Join-Path $Snapshot 'tools\quartus\shell_fit_reports.py'),
        '--git-capture', (Join-Path $Snapshot 'tools\quartus\capture_shell_fit_git.py'),
        '--runner', (Join-Path $Snapshot ($LauncherRel -replace '/', '\')),
        '--report-script', (Join-Path $Snapshot ($ReportScriptRel -replace '/', '\')),
        '--post-map-script', (Join-Path $Snapshot ($PostMapScriptRel -replace '/', '\')),
        '--project', (Join-Path $Snapshot ($QpfRel -replace '/', '\')),
        '--timing-metrics', (Join-Path $RawQuartus 'timing_metrics.tsv'),
        '--clock-transfers', (Join-Path $RawQuartus 'clock_transfers.rpt'),
        '--unconstrained-paths', (Join-Path $RawQuartus 'unconstrained_paths.rpt'),
        '--setup-paths', (Join-Path $RawQuartus 'setup_paths.rpt'),
        '--hold-paths', (Join-Path $RawQuartus 'hold_paths.rpt'),
        '--recovery-paths', (Join-Path $RawQuartus 'recovery_paths.rpt'),
        '--removal-paths', (Join-Path $RawQuartus 'removal_paths.rpt'),
        '--post-map-connectivity', (Join-Path $RawQuartus 'post_map_connectivity.tsv'),
        '--map-stdout', (Join-Path $StageLogs 'map.stdout.log'),
        '--map-stderr', (Join-Path $StageLogs 'map.stderr.log'),
        '--post-map-stdout', (Join-Path $StageLogs 'post-map.stdout.log'),
        '--post-map-stderr', (Join-Path $StageLogs 'post-map.stderr.log'),
        '--fit-stdout', (Join-Path $StageLogs 'fit.stdout.log'),
        '--fit-stderr', (Join-Path $StageLogs 'fit.stderr.log'),
        '--timequest-stdout', (Join-Path $StageLogs 'timequest.stdout.log'),
        '--timequest-stderr', (Join-Path $StageLogs 'timequest.stderr.log'),
        '--processors', "$Processors",
        '--evidence-mode', $EvidenceMode,
        '--git-head', (Join-Path $RawGit 'git-head.txt'),
        '--git-status', (Join-Path $RawGit 'git-status.txt'),
        '--git-worktree-diff', (Join-Path $RawGit 'git-worktree.diff'),
        '--git-staged-diff', (Join-Path $RawGit 'git-staged.diff'),
        '--git-index-flags', (Join-Path $RawGit 'git-index-flags.bin')
    )
    $ParserArgumentFile = Join-Path $RunRoot 'parser-arguments.txt'
    $ParserArgumentLines = [string[]]($ParserArgs | Select-Object -Skip 1)
    [IO.File]::WriteAllLines(
        $ParserArgumentFile,
        $ParserArgumentLines,
        [Text.UTF8Encoding]::new($false)
    )
    if ($TestOnlyFakeQuartus) {
        $TestOnlyParserLog = Join-Path $RunRoot 'test-only-parser.log'
        & $Python $ParserArgs[0] "@$ParserArgumentFile" *> $TestOnlyParserLog
    } else {
        & $Python $ParserArgs[0] "@$ParserArgumentFile"
    }
    $parserExitCode = $LASTEXITCODE
    if ($parserExitCode -notin @(0, 2)) {
        throw "shell-fit receipt derivation failed with exit code $parserExitCode."
    }
    if (-not (Test-Path -LiteralPath $ReceiptPath -PathType Leaf)) {
        throw 'Shell-fit parser did not publish a canonical receipt.'
    }
    Remove-Item -LiteralPath $ParserArgumentFile -Force
    $gatePassed = $parserExitCode -eq 0

    if ($TestOnlyFakeQuartus) {
        # Explicit fake-tool runs remain inside the disposable frozen workspace.
        # They never move a receipt into the live repository, update ledgers, or
        # print the production PASS token.
        $testResult = if ($gatePassed) { 'accepted' } else { 'rejected' }
        Write-Host "TEST-ONLY RESULT $testResult; no production evidence published."
        Write-Host "TEST_ONLY_RECEIPT $ReceiptPath"
        if (-not $gatePassed) {
            $scriptExitCode = 2
        }
    } else {
        $FinalRunRoot = Join-Path $ReportRoot "characterization\$GateName\$runId"
        if (Test-Path -LiteralPath $FinalRunRoot) {
            throw "Characterization run destination already exists: $FinalRunRoot"
        }
        New-Item -ItemType Directory -Path (Split-Path -Parent $FinalRunRoot) -Force | Out-Null
        [IO.Directory]::Move($RunRoot, $FinalRunRoot)

        $PublishedReceipt = Join-Path $FinalRunRoot 'receipt.json'
        $SynthesisLedger = Join-Path $ReportRoot 'synthesis\zhao_shell_fit.json'
        $TimingLedger = Join-Path $ReportRoot 'timing\zhao_shell_fit.json'
        # Each destination replacement is crash-safe. The canonical reader accepts
        # only stable byte-identical files, using the receipt hash as the pair
        # generation, so an interruption between these writes is UNKNOWN rather
        # than a consumable mixed current/stale pair.
        Publish-FileAtomic $PublishedReceipt $SynthesisLedger
        Publish-FileAtomic $PublishedReceipt $TimingLedger

        if ($gatePassed) {
            Write-Host "PASS $GateName at frozen source commit $head."
        } else {
            Write-Host "FAIL $GateName at frozen source commit $head; clean completed map/fit resources retained."
            $scriptExitCode = 2
        }
        Write-Host "RECEIPT $PublishedReceipt"
        Write-Host "SYNTHESIS_LEDGER $SynthesisLedger"
        Write-Host "TIMING_LEDGER $TimingLedger"
    }
    if ($KeepWorkspace) {
        Write-Host "WORKSPACE $Workspace"
    }
} finally {
    $env:GIT_DIR = $oldGitDir
    $env:GIT_WORK_TREE = $oldGitWorkTree
    $env:GIT_INDEX_FILE = $oldGitIndex
    if ($mutexHeld) {
        $mutex.ReleaseMutex()
    }
    $mutex.Dispose()
    if (-not $KeepWorkspace -and $null -ne $Workspace -and (Test-Path -LiteralPath $Workspace)) {
        Remove-Item -LiteralPath $Workspace -Recurse -Force
    }
    if ($null -ne $ArchiveZip -and (Test-Path -LiteralPath $ArchiveZip)) {
        Remove-Item -LiteralPath $ArchiveZip -Force
    }
}
exit $scriptExitCode
