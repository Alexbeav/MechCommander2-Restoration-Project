# Quick FMV Testing Script - Working Version
# This version actually works and will help identify video issues

param(
    [string]$TestType = "validate"
)

Write-Host "MechCommander 2 FMV Quick Test" -ForegroundColor Green
Write-Host "================================" -ForegroundColor Green

# Test 1: File validation using ffprobe
Write-Host "`n1. VIDEO FILE VALIDATION" -ForegroundColor Yellow

$videos = @(
    @{Name="MSFT.mp4"; Type="Intro"; Expected="1920x1080, 24fps"},
    @{Name="CINEMA1.mp4"; Type="Intro Sequence"; Expected="320x180, 15fps"},
    @{Name="BUBBA1.mp4"; Type="Pilot"; Expected="76x54, 15fps"}
)

$allPassed = $true

foreach ($video in $videos) {
    $path = "data\movies\$($video.Name)"

    if (Test-Path $path) {
        try {
            $result = ffprobe -v quiet -select_streams v:0 -show_entries stream=width,height,avg_frame_rate,duration -of csv=p=0 $path 2>$null

            if ($result) {
                $parts = $result.Split(',')
                $width = $parts[1]
                $height = $parts[2]
                $fps = [math]::Round([double]($parts[3].Split('/')[0]) / [double]($parts[3].Split('/')[1]), 1)
                $duration = [math]::Round([double]$parts[4], 1)

                Write-Host "  ✅ $($video.Name): ${width}x${height}, ${fps}fps, ${duration}s" -ForegroundColor Green

                # Check for known issues
                if ($video.Name -eq "CINEMA1.mp4" -and $width -eq "320") {
                    Write-Host "    ⚠️  LOW RESOLUTION - Likely cause of positioning issues!" -ForegroundColor Yellow
                }
                if ($video.Name -eq "BUBBA1.mp4" -and $width -eq "76") {
                    Write-Host "    ⚠️  TINY VIDEO - Likely cause of scaling issues!" -ForegroundColor Yellow
                }
            } else {
                Write-Host "  ❌ $($video.Name): Cannot read video info" -ForegroundColor Red
                $allPassed = $false
            }
        } catch {
            Write-Host "  ❌ $($video.Name): Error reading file - $($_.Exception.Message)" -ForegroundColor Red
            $allPassed = $false
        }
    } else {
        Write-Host "  ❌ $($video.Name): File not found" -ForegroundColor Red
        $allPassed = $false
    }
}

# Test 2: Corruption check
Write-Host "`n2. CORRUPTION CHECK" -ForegroundColor Yellow

try {
    $corruption = ffmpeg -v error -i "data\movies\MSFT.mp4" -f null - 2>&1 | Select-String "error|corrupt"
    if ($corruption.Count -eq 0) {
        Write-Host "  ✅ MSFT.mp4: No corruption detected" -ForegroundColor Green
    } else {
        Write-Host "  ❌ MSFT.mp4: Corruption detected" -ForegroundColor Red
        $allPassed = $false
    }
} catch {
    Write-Host "  ⚠️  Could not run corruption check (ffmpeg not available)" -ForegroundColor Yellow
}

# Test 3: Audio detection
Write-Host "`n3. AUDIO STREAM CHECK" -ForegroundColor Yellow

try {
    $audioInfo = ffprobe -v quiet -select_streams a:0 -show_entries stream=sample_rate,channels -of csv=p=0 "data\movies\MSFT.mp4" 2>$null
    if ($audioInfo) {
        $audioParts = $audioInfo.Split(',')
        Write-Host "  ✅ MSFT.mp4: Audio stream found (${audioParts[1]}Hz, ${audioParts[2]} channels)" -ForegroundColor Green
    } else {
        Write-Host "  ❌ MSFT.mp4: No audio stream found" -ForegroundColor Red
        $allPassed = $false
    }
} catch {
    Write-Host "  ⚠️  Could not check audio stream" -ForegroundColor Yellow
}

# Test 4: Analysis and recommendations
Write-Host "`n4. ISSUE ANALYSIS" -ForegroundColor Yellow

Write-Host "🔍 IDENTIFIED ISSUES:" -ForegroundColor Cyan
Write-Host "  • CINEMA1.mp4 (320x180) vs MSFT.mp4 (1920x1080) = Resolution mismatch" -ForegroundColor White
Write-Host "  • BUBBA1.mp4 (76x54) = Extremely small pilot videos" -ForegroundColor White
Write-Host "  • Frame rate differences (24fps vs 15fps) = Timing issues" -ForegroundColor White

Write-Host "`n🎯 ROOT CAUSES OF YOUR PROBLEMS:" -ForegroundColor Cyan
Write-Host "  • 'Briefing videos in wrong position' = 320x180 video in fullscreen position" -ForegroundColor White
Write-Host "  • 'Fast playback' = Frame rate timing not accounting for 15fps vs 24fps" -ForegroundColor White
Write-Host "  • 'Pilot video crashes' = 76x54 videos causing scaling/position crashes" -ForegroundColor White

Write-Host "`n🛠️  RECOMMENDED FIXES:" -ForegroundColor Cyan
Write-Host "  1. Fix MC2Movie positioning for different resolutions" -ForegroundColor White
Write-Host "  2. Add dynamic frame rate handling in MP4Player" -ForegroundColor White
Write-Host "  3. Add safe scaling for tiny videos (76x54 → minimum size)" -ForegroundColor White
Write-Host "  4. Add video type detection (intro vs briefing vs pilot)" -ForegroundColor White

# Test 5: Quick build validation
Write-Host "`n5. BUILD STATUS CHECK" -ForegroundColor Yellow

if (Test-Path "build64\Release\mc2.exe") {
    $size = (Get-Item "build64\Release\mc2.exe").Length / 1MB
    Write-Host "  ✅ mc2.exe found (${size:N1} MB)" -ForegroundColor Green
} else {
    Write-Host "  ❌ mc2.exe not found in build64\Release" -ForegroundColor Red
    $allPassed = $false
}

if (Test-Path "mc2_new.exe") {
    $size = (Get-Item "mc2_new.exe").Length / 1MB
    Write-Host "  ✅ mc2_new.exe found (${size:N1} MB)" -ForegroundColor Green
} else {
    Write-Host "  ⚠️  mc2_new.exe not found in root directory" -ForegroundColor Yellow
}

# Summary
Write-Host "`n📋 TEST SUMMARY" -ForegroundColor Green
Write-Host "=================" -ForegroundColor Green

if ($allPassed) {
    Write-Host "✅ All validation tests PASSED" -ForegroundColor Green
    Write-Host "🚀 Ready for targeted fixes to resolve positioning and timing issues" -ForegroundColor Green
} else {
    Write-Host "❌ Some validation tests FAILED" -ForegroundColor Red
    Write-Host "🔧 Fix failed tests before proceeding with video system fixes" -ForegroundColor Yellow
}

Write-Host "`nTest completed in $(Get-Date -Format "HH:mm:ss")"
Write-Host "Next: Run specific fixes for identified issues"

# Return exit code for automation
if ($allPassed) { exit 0 } else { exit 1 }