# pp_iec61131-3_samples.ps1 - Regenerate preprocessed (.piec) samples
# Usage: powershell .\tools\pp_iec61131-3_samples.ps1 [sample_name ...]
# If no sample name given, regenerate all samples.

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$SampleNames
)

# Auto-discover default samples from iec_61131-3/samples/*.iec
# Excludes files starting with _ (like _header.iec) and .piec preprocessed files
$DefaultSamples = @(Get-ChildItem -Path "iec_61131-3/samples/*.iec" -File |
    Where-Object { $_.Name -notlike '_*' -and $_.Name -notlike '*.piec' } |
    ForEach-Object { $_.BaseName } |
    Sort-Object)

# Samples that use {#syspath 'lib'} and need -I flag
$SampleWithLib = @('include', 'syspath')

# Use provided samples or all default samples
$samplesToBuild = if ($SampleNames -and $SampleNames.Count -gt 0) { $SampleNames } else { $DefaultSamples }

# Determine jiepp.exe path
$jiepp = $null
$debugExe = "build/windows-clang-ninja-debug/jiepp.exe"
$releaseExe = "build/windows-clang-ninja-release/jiepp.exe"

if (Test-Path $debugExe) {
    $jiepp = $debugExe
    Write-Host "Using debug jiepp: $jiepp"
} elseif (Test-Path $releaseExe) {
    $jiepp = $releaseExe
    Write-Host "Using release jiepp: $jiepp"
} else {
    Write-Host "ERROR: jiepp.exe not found in debug or release builds."
    Write-Host "Expected: $debugExe or $releaseExe"
    exit 1
}

# Track results
$successCount = 0
$failedSamples = @()

# Generate each sample
foreach ($sample in $samplesToBuild) {
    $inputFile = "iec_61131-3/samples/$sample.iec"
    $outputFile = "iec_61131-3/samples/$sample.piec"
    
    if (-not (Test-Path $inputFile)) {
        Write-Host "WARNING: Sample input not found: $inputFile"
        $failedSamples += "$sample (input file not found)"
        continue
    }
    
    # Build command with appropriate flags
    $cmdArgs = @($inputFile, "-o", $outputFile)
    
    # Add library include path for samples that use {#syspath 'lib'}
    if ($SampleWithLib -contains $sample) {
        $cmdArgs += "-I"
        $cmdArgs += "iec_61131-3/samples/lib"
    }
    
    # Execute
    Write-Host "Generating: $sample.piec"
    & $jiepp @cmdArgs
    
    if ($LASTEXITCODE -eq 0) {
        $successCount++
        Write-Host "  OK"
    } else {
        Write-Host "  FAILED (exit code: $LASTEXITCODE)"
        $failedSamples += $sample
    }
}

# Summary
Write-Host ""
Write-Host "======================================"
Write-Host "Summary: $successCount/$($samplesToBuild.Count) samples generated"

if ($failedSamples.Count -gt 0) {
    Write-Host "Failed samples:"
    foreach ($failed in $failedSamples) {
        Write-Host "  - $failed"
    }
    exit 1
} else {
    Write-Host "All samples generated successfully!"
    exit 0
}
