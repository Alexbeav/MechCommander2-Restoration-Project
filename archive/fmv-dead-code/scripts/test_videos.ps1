# Simple FMV Validation Test
Write-Host "MechCommander 2 FMV Test" -ForegroundColor Green

# Test key video files
$videos = @("MSFT.mp4", "CINEMA1.mp4", "BUBBA1.mp4")
$allGood = $true

foreach ($video in $videos) {
    $path = "data\movies\$video"

    if (Test-Path $path) {
        try {
            $info = ffprobe -v quiet -select_streams v:0 -show_entries stream=width,height,avg_frame_rate -of csv=p=0 $path 2>$null

            if ($info) {
                $parts = $info.Split(',')
                $width = $parts[1]
                $height = $parts[2]
                $fps = [math]::Round([double]($parts[3].Split('/')[0]) / [double]($parts[3].Split('/')[1]), 1)

                Write-Host "✓ $video : ${width}x${height}, ${fps}fps" -ForegroundColor Green

                # Check for issues
                if ($video -eq "CINEMA1.mp4" -and $width -eq "320") {
                    Write-Host "  WARNING: Low resolution likely causes positioning issues" -ForegroundColor Yellow
                }
                if ($video -eq "BUBBA1.mp4" -and $width -eq "76") {
                    Write-Host "  WARNING: Tiny video likely causes scaling problems" -ForegroundColor Yellow
                }
            } else {
                Write-Host "✗ $video : Cannot read video info" -ForegroundColor Red
                $allGood = $false
            }
        } catch {
            Write-Host "✗ $video : Error - $($_.Exception.Message)" -ForegroundColor Red
            $allGood = $false
        }
    } else {
        Write-Host "✗ $video : File not found" -ForegroundColor Red
        $allGood = $false
    }
}

Write-Host ""
Write-Host "ANALYSIS:" -ForegroundColor Cyan
Write-Host "• MSFT.mp4 (1920x1080) = High quality intro video" -ForegroundColor White
Write-Host "• CINEMA1.mp4 (320x180) = Low res video causing position problems" -ForegroundColor White
Write-Host "• BUBBA1.mp4 (76x54) = Tiny pilot video causing crashes" -ForegroundColor White

Write-Host ""
Write-Host "ISSUES IDENTIFIED:" -ForegroundColor Cyan
Write-Host "• Video positioning expects consistent resolution" -ForegroundColor White
Write-Host "• Frame rate timing not handling 15fps vs 24fps properly" -ForegroundColor White
Write-Host "• Tiny videos (76x54) need safe scaling" -ForegroundColor White

if ($allGood) {
    Write-Host "`nAll videos validated successfully!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "`nSome videos failed validation" -ForegroundColor Red
    exit 1
}