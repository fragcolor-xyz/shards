# setup-windows.ps1
# Windows development environment setup script for building Shards
# Run as Administrator in PowerShell
#
# === FRESH MACHINE INSTALL ===
# Open PowerShell as Administrator and paste this one-liner:
#
#   Set-ExecutionPolicy Bypass -Scope Process -Force; iwr -useb https://raw.githubusercontent.com/fragcolor-xyz/shards/devel/scripts/setup-windows.ps1 | iex
#
# Or manually download and run:
#   1. Open Edge/browser, go to: https://raw.githubusercontent.com/fragcolor-xyz/shards/devel/scripts/setup-windows.ps1
#   2. Save as setup-windows.ps1
#   3. Right-click PowerShell -> Run as Administrator
#   4. Run: Set-ExecutionPolicy Bypass -Scope Process -Force; .\setup-windows.ps1
#


$ErrorActionPreference = "Stop"

Write-Host "=== Shards Windows Development Environment Setup ===" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: This script must be run as Administrator" -ForegroundColor Red
    Write-Host "Right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

# Enable long paths in Windows registry (do this first, before git is installed)
Write-Host "[1/8] Enabling Windows long paths..." -ForegroundColor Green
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -Type DWord -Force
Write-Host "  Long paths enabled in registry" -ForegroundColor Gray

# Install Chocolatey if not present
Write-Host "[2/8] Installing Chocolatey..." -ForegroundColor Green
if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
    Set-ExecutionPolicy Bypass -Scope Process -Force
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
    Invoke-Expression ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

    # Refresh PATH
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
    Write-Host "  Chocolatey installed successfully" -ForegroundColor Gray
} else {
    Write-Host "  Chocolatey already installed" -ForegroundColor Gray
}

# Install Git
Write-Host "[3/8] Installing Git..." -ForegroundColor Green
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    choco install -y git
    # Refresh PATH
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
    Write-Host "  Git installed successfully" -ForegroundColor Gray
} else {
    Write-Host "  Git already installed: $(git --version)" -ForegroundColor Gray
}

# Configure git (now that it's installed)
Write-Host "[4/8] Configuring Git..." -ForegroundColor Green
git config --system core.longpaths true
git config --system core.autocrlf false
git config --system core.eol lf
Write-Host "  Git configured (long paths, LF line endings)" -ForegroundColor Gray

# Install Visual Studio Build Tools (provides Windows SDK and libs that clang needs)
Write-Host "[5/9] Installing Visual Studio Build Tools..." -ForegroundColor Green

# Detect architecture
$isArm64 = $env:PROCESSOR_ARCHITECTURE -eq "ARM64"
if ($isArm64) {
    Write-Host "  Detected ARM64 architecture" -ForegroundColor Gray
    $vcComponent = "Microsoft.VisualStudio.Component.VC.Tools.ARM64"
} else {
    $vcComponent = "Microsoft.VisualStudio.Component.VC.Tools.x86.x64"
}

$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsInstalled = & $vsWhere -products * -requires $vcComponent -property installationPath 2>$null
    if ($vsInstalled) {
        Write-Host "  Visual Studio Build Tools already installed" -ForegroundColor Gray
    } else {
        if ($isArm64) {
            choco install -y visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended --passive --norestart"
        } else {
            choco install -y visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended --passive --norestart"
        }
        Write-Host "  VS Build Tools installed" -ForegroundColor Gray
    }
} else {
    if ($isArm64) {
        choco install -y visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.ARM64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended --passive --norestart"
    } else {
        choco install -y visualstudio2022buildtools --package-parameters "--add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended --passive --norestart"
    }
    Write-Host "  VS Build Tools installed" -ForegroundColor Gray
}

# Install LLVM (specific version matching CI)
Write-Host "[6/9] Installing LLVM 20.1.7..." -ForegroundColor Green
$llvmVersion = choco list llvm --local-only --exact --limit-output 2>$null
if ($llvmVersion -match "20.1.7") {
    Write-Host "  LLVM 20.1.7 already installed" -ForegroundColor Gray
} else {
    choco install -y --force --version=20.1.7 llvm
    Write-Host "  LLVM installed at: C:\Program Files\LLVM\bin" -ForegroundColor Gray
}
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")

# Install Ninja
Write-Host "[7/9] Installing Ninja..." -ForegroundColor Green
choco install -y ninja
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
Write-Host "  Ninja installed successfully" -ForegroundColor Gray

# Install CMake (usually present but let's ensure)
Write-Host "[8/9] Installing/Updating CMake..." -ForegroundColor Green
choco install -y cmake --installargs 'ADD_CMAKE_TO_PATH=System'
$env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
Write-Host "  CMake version: $(cmake --version | Select-Object -First 1)" -ForegroundColor Gray

# Install Rust
Write-Host "[9/9] Installing Rust..." -ForegroundColor Green
$rustToolchain = "nightly-2025-08-01-msvc"

if (-not (Get-Command rustup -ErrorAction SilentlyContinue)) {
    # Download and run rustup-init
    $rustupInit = "$env:TEMP\rustup-init.exe"
    Invoke-WebRequest -Uri "https://win.rustup.rs/x86_64" -OutFile $rustupInit
    & $rustupInit -y --default-toolchain $rustToolchain
    Remove-Item $rustupInit -Force -ErrorAction SilentlyContinue

    # Add cargo to PATH for current session
    $env:Path = "$env:USERPROFILE\.cargo\bin;" + $env:Path
    Write-Host "  Rust installed with toolchain: $rustToolchain" -ForegroundColor Gray
} else {
    Write-Host "  Rustup already installed, installing toolchain..." -ForegroundColor Gray
    rustup toolchain install $rustToolchain
}

# Set the toolchain as default for this project
rustup default $rustToolchain

Write-Host ""
Write-Host "=== Installation Complete ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "Installed components:" -ForegroundColor Yellow
Write-Host "  - Chocolatey (package manager)"
Write-Host "  - Git (with long paths enabled)"
Write-Host "  - Visual Studio Build Tools 2022 (Windows SDK + libs)"
Write-Host "  - LLVM 20.1.7 (clang/clang++)"
Write-Host "  - Ninja (build system)"
Write-Host "  - CMake"
Write-Host "  - Rust $rustToolchain"
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "  1. Open 'Developer PowerShell for VS 2022' from Start Menu" -ForegroundColor White
Write-Host "     (This sets up link.exe and other VS tools in PATH)" -ForegroundColor Gray
Write-Host "  2. Clone the repository:"
Write-Host "     git clone --recursive https://github.com/fragcolor-xyz/shards.git"
Write-Host "  3. Build:"
Write-Host "     cd shards"
Write-Host "     mkdir build; cd build"
Write-Host '     cmake -G Ninja -DCMAKE_C_COMPILER="C:\Program Files\LLVM\bin\clang.exe" -DCMAKE_CXX_COMPILER="C:\Program Files\LLVM\bin\clang++.exe" -DCMAKE_BUILD_TYPE=Debug ..'
Write-Host "     ninja shards"
Write-Host ""

Write-Host "IMPORTANT: Use 'Developer PowerShell for VS 2022' (not regular PowerShell) for building!" -ForegroundColor Yellow
