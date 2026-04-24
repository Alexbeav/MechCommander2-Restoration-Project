# MechCommander 2 FMV Testing Framework

## Overview
This automated testing framework helps you quickly identify and fix video playback issues in MechCommander 2. It addresses your current problems:

- **Timing Issues**: Fast playback, A/V sync drift
- **Positioning Problems**: Briefing videos in wrong locations
- **OpenGL Failures**: Texture creation errors
- **Sequence Issues**: Intro video chain, looping problems
- **Crashes**: Pilot videos, memory leaks

## Quick Start

### 1. Build the Test Framework
```bash
# Add test framework to your CMakeLists.txt
cat CMakeListsTestAddition.txt >> CMakeLists.txt

# Build with tests
cd build64
cmake --build . --config Release
```

### 2. Run Tests

**Fast validation (30 seconds):**
```powershell
.\test_fmv.ps1 quick
```

**Test intro sequence:**
```powershell
.\test_fmv.ps1 intro -Verbose
```

**Test specific video:**
```powershell
.\test_fmv.ps1 specific "data\movies\MSFT.mp4" -Visual
```

**Full test suite:**
```powershell
.\test_fmv.ps1 all
```

## Test Types

### 1. File Validation (`quick`)
- **Speed**: Very fast (seconds)
- **Checks**: File integrity, codecs, resolution, frame rate
- **Use**: Before full testing, CI pipelines
- **Command**: `.\test_fmv.ps1 quick`

### 2. Intro Sequence (`intro`)
- **Speed**: 1-2 minutes
- **Checks**: MSFT.mp4 → CINEMA1.mp4 sequence, timing, A/V sync
- **Use**: After intro video changes
- **Command**: `.\test_fmv.ps1 intro`

### 3. Briefing Videos (`all`)
- **Speed**: Varies by video count
- **Checks**: Positioning, looping, crashes
- **Use**: After UI or video system changes
- **Command**: `.\test_fmv.ps1 all`

### 4. Stress Testing (`stress`)
- **Speed**: 5+ minutes
- **Checks**: Memory leaks, rapid start/stop, concurrent videos
- **Use**: Before releases, after major changes
- **Command**: `.\test_fmv.ps1 stress`

## Understanding Test Results

### Pass/Fail Criteria
✅ **PASS** if:
- Timing error < 5%
- A/V drift < 100ms
- No OpenGL errors
- Correct positioning
- No crashes

❌ **FAIL** if:
- File corruption detected
- Timing error > 5%
- A/V sync violations > 10
- Texture creation fails
- Crashes or exceptions

### Reports Generated

**HTML Report** (`fmv_test_report.html`):
- Visual dashboard with pass/fail status
- Detailed metrics for each video
- Performance graphs and timing analysis

**CI Summary** (`test_results/fmv_ci_summary.json`):
- Machine-readable results for automation
- Exit codes for build pipelines

**JUnit Report** (`test_results/fmv_junit.xml`):
- Integration with CI systems (Jenkins, GitHub Actions)

## Integration with Development

### Daily Development
```powershell
# Before committing video changes
.\test_fmv.ps1 quick

# If quick tests pass, run full validation
.\test_fmv.ps1 intro
```

### CI/CD Pipeline
```yaml
# GitHub Actions example
- name: Test FMV System
  run: |
    .\test_fmv.ps1 quick
    if ($LASTEXITCODE -eq 0) {
      .\test_fmv.ps1 intro
    }
```

### Debugging Specific Issues

**Timing Problems:**
```powershell
# Test with visual output to see timing
.\test_fmv.ps1 specific "data\movies\MSFT.mp4" -Visual -Verbose
```

**Positioning Issues:**
```powershell
# Test briefing videos with visual feedback
.\test_fmv.ps1 all -Visual
```

**Memory Leaks:**
```powershell
# Run stress tests
.\test_fmv.ps1 stress -Verbose
```

## Adding Custom Tests

### Test New Video Files
1. Place video in `data/movies/`
2. Test individually:
   ```powershell
   .\test_fmv.ps1 specific "data\movies\new_video.mp4"
   ```

### Custom Test Scenarios
Edit `fmv_test_framework.cpp` to add:
- New video directories
- Custom positioning checks
- Specific timing requirements

### Integration with Game Code
Add test metrics to your video playback code:

```cpp
#include "mp4player_test_extension.h"

// In your MP4Player methods:
void MP4Player::update() {
    // ... existing code ...

    // Add test metrics
    TEST_METRIC_VIDEO_DECODED(pts, decode_time_ms);
    TEST_METRIC_AUDIO_PROCESSED(audio_pts, queue_size);
}

void MC2Movie::render() {
    // ... existing code ...

    // Add test metrics
    TEST_METRIC_VIDEO_RENDERED(pts, render_time_ms);
    TEST_METRIC_GL_ERROR("render", glGetError());
}
```

## Troubleshooting

### Common Issues

**"Test runner not found":**
```bash
# Rebuild the test framework
cd build64
cmake --build . --config Release --target fmv_test_runner
```

**"Video files not found":**
```bash
# Copy videos from full_game if needed
copy "full_game\data\movies\*" "data\movies\"
```

**Tests timeout:**
```bash
# Increase timeout in CMakeLists.txt or run individually
.\test_fmv.ps1 quick  # Start with fastest tests
```

### Performance Optimization

**For faster iteration:**
1. Use `--quick` validation during development
2. Run full tests only before commits
3. Use headless mode for CI (default)
4. Test specific files when debugging

**For comprehensive testing:**
1. Run stress tests before releases
2. Use visual mode for debugging positioning
3. Generate HTML reports for analysis
4. Monitor memory usage trends

## Expected Impact

### Before Framework
- ❌ Manual testing of each video
- ❌ Timing issues discovered late
- ❌ Positioning problems missed
- ❌ Memory leaks undetected
- ❌ Regression testing difficult

### After Framework
- ✅ **30-second validation** catches file issues
- ✅ **Automated A/V sync testing** detects timing problems
- ✅ **Position validation** ensures UI correctness
- ✅ **Memory leak detection** prevents performance issues
- ✅ **CI integration** prevents regressions

This framework will dramatically speed up your video system development and help you quickly identify the root causes of issues like the ones mentioned in your git commits ("buggy progress", "very buggy", "still super fast forward").

## Next Steps

1. **Build and test the framework** with your current videos
2. **Integrate metrics** into your MP4Player code
3. **Add to CI pipeline** for automated testing
4. **Extend tests** for new video scenarios as needed

The framework is designed to grow with your project and can be easily extended for new video types or testing scenarios.