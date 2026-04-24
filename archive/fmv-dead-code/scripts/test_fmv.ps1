# FMV Testing Script for MechCommander 2
# Usage: .\test_fmv.ps1 [test_type]

param(
    [Parameter(Position=0)]
    [ValidateSet("quick", "intro", "all", "stress", "specific")]
    [string]$TestType = "quick",

    [Parameter(Position=1)]
    [string]$VideoFile = "",

    [switch]$Visual = $false,
    [switch]$Verbose = $false,
    [switch]$Help = $false
)

if ($Help) {
    Write-Host "FMV Testing Script for MechCommander 2"
    Write-Host "======================================"
    Write-Host ""
    Write-Host "Usage: .\test_fmv.ps1 [test_type] [options]"
    Write-Host ""
    Write-Host "Test Types:"
    Write-Host "  quick      - Fast file validation only (default)"
    Write-Host "  intro      - Test intro sequence (MSFT.mp4 -> CINEMA1.mp4)"
    Write-Host "  all        - Run all tests"
    Write-Host "  stress     - Stress testing (memory leaks, rapid cycles)"
    Write-Host "  specific   - Test specific video file"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Visual    - Show video output during testing"
    Write-Host "  -Verbose   - Detailed output"
    Write-Host "  -Help      - Show this help"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\test_fmv.ps1 quick                        # Fast validation"
    Write-Host "  .\test_fmv.ps1 intro -Verbose              # Test intro with details"
    Write-Host "  .\test_fmv.ps1 specific video.mp4 -Visual  # Test specific file"
    exit 0
}

# Check if build exists
$TestRunner = "build64\Release\fmv_test_runner.exe"
if (-not (Test-Path $TestRunner)) {
    Write-Host "ERROR: Test runner not found at $TestRunner"
    Write-Host "Please build the project first:"
    Write-Host "  cd build64"
    Write-Host "  cmake --build . --config Release"
    exit 1
}

# Check if video files exist
$VideoDir = "data\movies"
if (-not (Test-Path $VideoDir)) {
    Write-Host "WARNING: Video directory not found at $VideoDir"
    Write-Host "Some tests may fail if video files are missing."
}

# Build command line arguments
$Args = @()

switch ($TestType) {
    "quick"    { $Args += "--quick"; $Args += "--suite"; $Args += "validation" }
    "intro"    { $Args += "--suite"; $Args += "intro" }
    "all"      { $Args += "--suite"; $Args += "all" }
    "stress"   { $Args += "--suite"; $Args += "stress" }
    "specific" {
        if ([string]::IsNullOrEmpty($VideoFile)) {
            Write-Host "ERROR: Video file must be specified for specific test"
            Write-Host "Usage: .\test_fmv.ps1 specific path\to\video.mp4"
            exit 1
        }
        $Args += "--file"; $Args += $VideoFile
    }
}

if ($Visual) {
    $Args += "--visual"
} else {
    $Args += "--headless"
}

if ($Verbose) {
    $Args += "--verbose"
}

# Add output directory
$Args += "--output"
$Args += "test_results"

# Create output directory
if (-not (Test-Path "test_results")) {
    New-Item -ItemType Directory -Path "test_results" | Out-Null
}

Write-Host "Running FMV tests..."
Write-Host "Test Type: $TestType"
if ($Verbose) {
    Write-Host "Command: $TestRunner $($Args -join ' ')"
}
Write-Host ""

# Run the test
$StartTime = Get-Date
try {
    & $TestRunner @Args
    $ExitCode = $LASTEXITCODE
} catch {
    Write-Host "ERROR: Failed to run test runner: $_"
    exit 1
}

$EndTime = Get-Date
$Duration = $EndTime - $StartTime

Write-Host ""
Write-Host "Test completed in $($Duration.TotalSeconds) seconds"

# Show results
if (Test-Path "fmv_test_report.html") {
    Write-Host "HTML Report: fmv_test_report.html"
}

if (Test-Path "test_results\fmv_ci_summary.json") {
    Write-Host "CI Summary: test_results\fmv_ci_summary.json"
}

if (Test-Path "test_results\fmv_junit.xml") {
    Write-Host "JUnit Report: test_results\fmv_junit.xml"
}

# Parse CI summary for quick results
if (Test-Path "test_results\fmv_ci_summary.json") {
    try {
        $Summary = Get-Content "test_results\fmv_ci_summary.json" | ConvertFrom-Json
        Write-Host ""
        Write-Host "Quick Summary:"
        Write-Host "  Total Tests: $($Summary.total_tests)"
        Write-Host "  Passed: $($Summary.passed)"
        Write-Host "  Failed: $($Summary.failed)"
        Write-Host "  Success Rate: $($Summary.success_rate)%"

        if ($Summary.failed -gt 0) {
            Write-Host ""
            Write-Host "Failed Tests:" -ForegroundColor Red
            foreach ($result in $Summary.results) {
                if (-not $result.passed) {
                    Write-Host "  - $($result.filename)" -ForegroundColor Red
                    foreach ($reason in $result.failure_reasons) {
                        Write-Host "    $reason" -ForegroundColor Yellow
                    }
                }
            }
        }
    } catch {
        Write-Host "Could not parse CI summary"
    }
}

# Open HTML report if available and requested
if ($Visual -and (Test-Path "fmv_test_report.html")) {
    Write-Host ""
    Write-Host "Opening HTML report in browser..."
    Start-Process "fmv_test_report.html"
}

exit $ExitCode