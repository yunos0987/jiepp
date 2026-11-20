# pp_iec61131-3_samples.ps1 - Regenerate preprocessed (.piec) samples
# Usage: powershell .\tools\pp_iec61131-3_samples.ps1 [sample_name ...]
# If no sample name given, regenerate all samples.
# Can be started from any directory: it works from the repository root (the
# parent of this script's directory), because the line markers in a .piec
# hold the input path as given, relative to the repository root.

param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$SampleNames
)

$exitCode = 0
$tempFile = $null
Push-Location (Split-Path -Parent $PSScriptRoot)
try {
    # Auto-discover default samples from iec_61131-3/samples/*.iec
    # Excludes files starting with _ (like _header.iec), which are include fragments
    $DefaultSamples = @(Get-ChildItem -Path "iec_61131-3/samples/*.iec" -File |
        Where-Object { $_.Name -notlike '_*' } |
        ForEach-Object { $_.BaseName } |
        Sort-Object)

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
        $exitCode = 1
    }

    if ($jiepp) {
        # Track results
        $successCount = 0
        $failedSamples = @()

        # Generate each sample
        foreach ($sample in $samplesToBuild) {
            $inputFile = "iec_61131-3/samples/$sample.iec"
            $outputFile = "iec_61131-3/samples/$sample.piec"
            # jiepp writes -o only when it succeeds; the temporary name also
            # keeps the checked-in .piec intact if the run is interrupted.
            $tempFile = "$outputFile.tmp"

            if (-not (Test-Path $inputFile)) {
                Write-Host "WARNING: Sample input not found: $inputFile"
                $failedSamples += "$sample (input file not found)"
                continue
            }

            # Build command with appropriate flags. {#syspath} directives inside a
            # sample resolve relative to the sample's own directory, so no -I flag
            # is needed here even for samples that use {#syspath 'lib'}.
            $cmdArgs = @($inputFile, "-o", $tempFile)

            # Execute
            Write-Host "Generating: $sample.piec"
            & $jiepp @cmdArgs

            if ($LASTEXITCODE -eq 0 -and (Test-Path $tempFile)) {
                Move-Item -Force -Path $tempFile -Destination $outputFile
                $successCount++
                Write-Host "  OK"
            } else {
                Write-Host "  FAILED (exit code: $LASTEXITCODE)"
                if (Test-Path $tempFile) { Remove-Item -Force -Path $tempFile }
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
            $exitCode = 1
        } else {
            Write-Host "All samples generated successfully!"
        }
    }
} finally {
    # An interrupted run must not leave a .piec.tmp in the samples directory.
    if ($tempFile -and (Test-Path $tempFile)) { Remove-Item -Force -Path $tempFile }
    Pop-Location
}
exit $exitCode
