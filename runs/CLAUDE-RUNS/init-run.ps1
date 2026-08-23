<#
.SYNOPSIS
    Initialize a new Claude Code run directory.

.DESCRIPTION
    Creates RUN-YYYYMMDD-HHMM-<slug>/ containing TASK_LOG.md and SPEC_v1.md,
    generated from the templates in docs/coding_agents/claude_run_templates/.

    PowerShell port of the original init-run.sh. Differences from the bash version:
      - Timestamps use the machine's local timezone as a UTC offset rather than a
        hardcoded "EST", so the log says what it means wherever it is run.
      - Files are written UTF-8 without BOM; Set-Content's default ANSI codepage
        would mangle the non-ASCII characters in the templates.

.PARAMETER Slug
    Short kebab-case name for the run, e.g. fix-auth-bug.

.EXAMPLE
    .\init-run.ps1 fix-auth-bug
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $false, Position = 0)]
    [string]$Slug
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Slug)) {
    Write-Host "Usage: .\init-run.ps1 <slug>"
    Write-Host "Example: .\init-run.ps1 fix-auth-bug"
    exit 1
}

$scriptDir   = $PSScriptRoot
$repoRoot    = (Resolve-Path (Join-Path $scriptDir '..\..')).Path
$templateDir = Join-Path $repoRoot 'docs\coding_agents\claude_run_templates'

$now         = Get-Date
$runId       = $now.ToString('yyyyMMdd-HHmm')
$timestamp   = $now.ToString("yyyy-MM-dd HH:mm 'UTC'zzz")
$description = '[Describe objective here]'

$runDir = Join-Path $scriptDir "RUN-$runId-$Slug"

if (Test-Path $runDir) {
    Write-Error "Directory already exists: $runDir"
    exit 1
}

New-Item -ItemType Directory -Path $runDir | Out-Null

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Expand-Template {
    param(
        [string]$TemplatePath,
        [string]$OutputPath,
        [string]$Label
    )
    if (-not (Test-Path $TemplatePath)) {
        Write-Warning "$Label template not found at $TemplatePath"
        return $false
    }
    $content = [System.IO.File]::ReadAllText($TemplatePath)
    # Literal .Replace(), not -replace: the {{...}} markers would otherwise be
    # read as regex quantifiers.
    $content = $content.Replace('{{RUN_ID}}',      $runId)
    $content = $content.Replace('{{SLUG}}',        $Slug)
    $content = $content.Replace('{{TIMESTAMP}}',   $timestamp)
    $content = $content.Replace('{{DESCRIPTION}}', $description)
    [System.IO.File]::WriteAllText($OutputPath, $content, $utf8NoBom)
    return $true
}

$madeTaskLog = Expand-Template -TemplatePath (Join-Path $templateDir 'TASK_LOG\TASK_LOG.md') `
                               -OutputPath   (Join-Path $runDir 'TASK_LOG.md') `
                               -Label        'TASK_LOG'

$madeSpec    = Expand-Template -TemplatePath (Join-Path $templateDir 'SPEC\SPEC_v1.md') `
                               -OutputPath   (Join-Path $runDir 'SPEC_v1.md') `
                               -Label        'SPEC'

Write-Host "Created: $runDir"
if ($madeTaskLog) { Write-Host "   TASK_LOG.md" }
if ($madeSpec)    { Write-Host "   SPEC_v1.md" }
Write-Host ""
Write-Host "Run ID: RUN-$runId"
