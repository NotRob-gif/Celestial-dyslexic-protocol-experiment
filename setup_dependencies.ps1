$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$TempRoot = Join-Path $env:TEMP ("cdp-spice-" + [guid]::NewGuid().ToString())

$ToolkitUrl = "https://naif.jpl.nasa.gov/pub/naif/toolkit/C/PC_Windows_VisualC_64bit/packages/cspice.zip"
$LskUrl     = "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/lsk/naif0012.tls"
$SpkUrl     = "https://naif.jpl.nasa.gov/pub/naif/generic_kernels/spk/planets/de440.bsp"

$CspiceDir  = Join-Path $Root "cspice"
$KernelsDir = Join-Path $Root "kernels"

function Download-File {
    param(
        [Parameter(Mandatory=$true)][string]$Url,
        [Parameter(Mandatory=$true)][string]$Destination
    )

    Write-Host "Downloading: $Url"

    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($curl) {
        & curl.exe -L --fail --retry 3 --output $Destination $Url
        if ($LASTEXITCODE -ne 0) {
            throw "Download failed: $Url"
        }
    }
    else {
        Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing
    }
}

try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    New-Item -ItemType Directory -Path $TempRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $KernelsDir -Force | Out-Null

    $NeedToolkit = -not (
        (Test-Path (Join-Path $CspiceDir "include\SpiceUsr.h")) -and
        (Test-Path (Join-Path $CspiceDir "lib\cspice.lib")) -and
        (Test-Path (Join-Path $CspiceDir "lib\csupport.lib"))
    )

    if ($NeedToolkit) {
        $ToolkitZip = Join-Path $TempRoot "cspice.zip"
        Download-File -Url $ToolkitUrl -Destination $ToolkitZip

        Write-Host "Extracting CSPICE..."
        Expand-Archive -Path $ToolkitZip -DestinationPath $TempRoot -Force

        $ExtractedCspice = Join-Path $TempRoot "cspice"
        if (-not (Test-Path $ExtractedCspice)) {
            $Candidate = Get-ChildItem -Path $TempRoot -Directory -Recurse |
                Where-Object { $_.Name -eq "cspice" } |
                Select-Object -First 1

            if ($null -eq $Candidate) {
                throw "Could not find the cspice folder after extracting the toolkit."
            }

            $ExtractedCspice = $Candidate.FullName
        }

        if (Test-Path $CspiceDir) {
            Remove-Item -Path $CspiceDir -Recurse -Force
        }

        Copy-Item -Path $ExtractedCspice -Destination $CspiceDir -Recurse -Force
    }
    else {
        Write-Host "CSPICE is already installed in this repository. Skipping toolkit download."
    }

    $LskPath = Join-Path $KernelsDir "naif0012.tls"
    if (-not (Test-Path $LskPath)) {
        Download-File -Url $LskUrl -Destination $LskPath
    }
    else {
        Write-Host "naif0012.tls already exists. Skipping."
    }

    $SpkPath = Join-Path $KernelsDir "de440.bsp"
    if (-not (Test-Path $SpkPath)) {
        Write-Host "DE440 is about 114 MB, so this download may take a little while."
        Download-File -Url $SpkUrl -Destination $SpkPath
    }
    else {
        Write-Host "de440.bsp already exists. Skipping."
    }

    $RequiredFiles = @(
        (Join-Path $CspiceDir "include\SpiceUsr.h"),
        (Join-Path $CspiceDir "lib\cspice.lib"),
        (Join-Path $CspiceDir "lib\csupport.lib"),
        $LskPath,
        $SpkPath
    )

    foreach ($File in $RequiredFiles) {
        if (-not (Test-Path $File)) {
            throw "Setup did not create required file: $File"
        }
    }

    Write-Host ""
    Write-Host "Setup complete." -ForegroundColor Green
    Write-Host "Open CDP-Experiment.slnx in Visual Studio, select x64, then build/run."
}
catch {
    Write-Host ""
    Write-Host "Dependency setup failed:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
finally {
    if (Test-Path $TempRoot) {
        Remove-Item -Path $TempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
