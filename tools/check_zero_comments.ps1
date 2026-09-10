$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir

$scanDirs = @(
    (Join-Path $rootDir "src"),
    (Join-Path $rootDir "tests")
)

$violations = @()

foreach ($dir in $scanDirs) {
    if (Test-Path $dir) {
        Get-ChildItem -Path $dir -Recurse -Include "*.c", "*.h" | ForEach-Object {
            $file = $_.FullName
            $relPath = $file.Substring($rootDir.Length + 1)
            $lines = Get-Content -Path $file
            for ($i = 0; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -match '(//|/\*|\*/)') {
                    $violations += [PSCustomObject]@{
                        File = $relPath
                        Line = $i + 1
                        Text = $lines[$i].Trim()
                    }
                }
            }
        }
    }
}

if ($violations.Count -gt 0) {
    Write-Host "Hard Zero Comments Rule VIOLATIONS FOUND ($($violations.Count) occurrences):" -ForegroundColor Red
    foreach ($v in $violations) {
        Write-Host "  $($v.File):$($v.Line): $($v.Text)" -ForegroundColor Yellow
    }
    exit 1
}

Write-Host "Hard Zero Comments Audit PASSED (100% compliant across src/ and tests/)." -ForegroundColor Green
exit 0
