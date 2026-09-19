# Windows wrapper for qwen_job.py: runs it inside the HomeAI WSL distro, which
# holds the broker secret. Usage mirrors qwen_job.py:
#   qwen.ps1 run <job.md> [--dry] | new <dir> <slug> [--continue Qnnn] [--review Qnnn] | verdict <dir> Qnnn <v> "<note>"
$script = Join-Path $PSScriptRoot 'qwen_job.py'
$m = [regex]::Match($script, '^([A-Za-z]):\\(.*)$')
$wslScript = '/mnt/' + $m.Groups[1].Value.ToLower() + '/' + ($m.Groups[2].Value -replace '\\', '/')
wsl.exe -d HomeAI -u root --exec env PYTHONPATH=/opt/homeai/app/src PYTHONIOENCODING=utf-8 /opt/homeai/venv/bin/python $wslScript @args
exit $LASTEXITCODE
