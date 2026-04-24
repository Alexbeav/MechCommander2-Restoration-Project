$ErrorActionPreference = "Stop"

$root = "F:\games source code\games\mc2"
$exe = Join-Path $root "build64\Release\mp4_standalone.exe"
$logDir = Join-Path $root "build64\Release\mp4_logs"

New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$videos = @(
    @{ Name = "MSFT";    Path = Join-Path $root "full_game\data\movies\MSFT.mp4";    MinFrames = 5; Duration = 8 },
    @{ Name = "CINEMA1"; Path = Join-Path $root "full_game\data\movies\CINEMA1.mp4"; MinFrames = 5; Duration = 8 },
    @{ Name = "BUBBA1";  Path = Join-Path $root "full_game\data\movies\BUBBA1.mp4";  MinFrames = 3; Duration = 6 }
)

$failed = $false

foreach ($v in $videos) {
    if (-not (Test-Path $v.Path)) {
        Write-Host "SKIP $($v.Name) - missing: $($v.Path)"
        continue
    }

    $log = Join-Path $logDir "$($v.Name).log"
    Write-Host "RUN  $($v.Name) -> $log"

    & $exe $v.Path --duration $v.Duration --expect_frames $v.MinFrames --log $log
    $code = $LASTEXITCODE

    if ($code -eq 0) {
        Write-Host "PASS $($v.Name)"
    } else {
        Write-Host "FAIL $($v.Name) (exit $code)"
        $failed = $true
    }
}

if ($failed) {
    exit 1
}
exit 0
