param(
    [switch]$Once,
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"

$previewDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectDir = Split-Path -Parent $previewDir
$pio = Join-Path $env:APPDATA "Python\Python312\Scripts\pio.exe"

if (-not (Test-Path -LiteralPath $pio)) {
    $pioCommand = Get-Command pio -ErrorAction SilentlyContinue
    if ($pioCommand) {
        $pio = $pioCommand.Source
    }
}

if (-not (Test-Path -LiteralPath $pio)) {
    throw "PlatformIO (pio.exe) was not found. Install PlatformIO or add pio to PATH."
}

$exe = Join-Path $previewDir ".pio\build\preview_final\native\program.exe"
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
$sourceRoots = @(
    (Join-Path $projectDir "main\ui"),
    (Join-Path $projectDir "main\app"),
    (Join-Path $projectDir "main\input"),
    (Join-Path $previewDir "src"),
    (Join-Path $previewDir "include")
)
$sourceExtensions = @(".c", ".cc", ".cpp", ".h", ".hpp", ".ini")
$previewProcess = $null

function Get-SourceStamp {
    $files = foreach ($root in $sourceRoots) {
        if (Test-Path -LiteralPath $root) {
            Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
                Where-Object { $sourceExtensions -contains $_.Extension.ToLowerInvariant() }
        }
    }

    if (-not $files) {
        return [DateTime]::MinValue
    }

    return ($files | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).LastWriteTimeUtc
}

function Stop-Preview {
    if ($script:previewProcess -and -not $script:previewProcess.HasExited) {
        Stop-Process -Id $script:previewProcess.Id -Force -ErrorAction SilentlyContinue
        $script:previewProcess = $null
    }
}

function Build-AndStart {
    # Windows cannot replace a running executable. Stop our own preview before
    # linking, then restore the previous binary if the new build fails.
    Stop-Preview
    if (-not $NoBuild) {
        Write-Host "`n[LVGL] Building desktop preview..." -ForegroundColor Cyan
        & $pio run -d $previewDir
        if ($LASTEXITCODE -ne 0) {
            if (Test-Path -LiteralPath $exe) {
                $script:previewProcess = Start-Process -FilePath $exe `
                    -ArgumentList "--scale=1.00" `
                    -WorkingDirectory $previewDir `
                    -PassThru
            }
            Write-Warning "Build failed. The previous preview binary was restored; the watcher will try again after the next save."
            return $false
        }
    }

    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Preview executable does not exist: $exe"
    }

    $script:previewProcess = Start-Process -FilePath $exe `
        -ArgumentList "--scale=1.00" `
        -WorkingDirectory $previewDir `
        -PassThru
    Write-Host "[LVGL] Preview opened at 540 x 960 (100%)." -ForegroundColor Green
    return $true
}

try {
    Build-AndStart | Out-Null
    $lastStamp = Get-SourceStamp

    if ($Once) {
        exit 0
    }

    Write-Host "[LVGL] Watching UI source files. Saving a file rebuilds and refreshes the preview." -ForegroundColor Yellow
    Write-Host "[LVGL] Close this console to stop the live preview." -ForegroundColor DarkGray

    while ($true) {
        Start-Sleep -Milliseconds 700

        if ($script:previewProcess -and $script:previewProcess.HasExited) {
            Write-Host "[LVGL] Preview window closed; reopening..." -ForegroundColor DarkGray
            Build-AndStart | Out-Null
        }

        $currentStamp = Get-SourceStamp
        if ($currentStamp -gt $lastStamp) {
            Start-Sleep -Milliseconds 250
            $lastStamp = Get-SourceStamp
            Build-AndStart | Out-Null
        }
    }
}
finally {
    Stop-Preview
}
