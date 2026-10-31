param (
    [Parameter(Mandatory=$false)]
    [string]$SourceIso = ""
)

$ErrorActionPreference = "Continue"

# 1. Locate VBoxManage executable
$VBoxManage = if (Get-Command VBoxManage -ErrorAction SilentlyContinue) {
    "VBoxManage"
} elseif (Test-Path "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe") {
    "C:\Program Files\Oracle\VirtualBox\VBoxManage.exe"
} elseif (Test-Path "C:\Program Files (x86)\Oracle\VirtualBox\VBoxManage.exe") {
    "C:\Program Files (x86)\Oracle\VirtualBox\VBoxManage.exe"
} else {
    Write-Host "[ERROR] VBoxManage.exe not found! Please install VirtualBox or add it to PATH."
    exit 1
}

$VmName = "OSProject"
$DestDir = "C:\os"
$DestIso = "C:\os\myos.iso"

# 2. Check VM status and power off if currently running
try {
    $info = (& $VBoxManage showvminfo $VmName --machinereadable 2>&1) -join "`n"
    if ($info -match 'VMState="([^"]+)"') {
        $state = $matches[1]
        if ($state -ne "poweroff" -and $state -ne "saved") {
            Write-Host "[RUN] VM '$VmName' is in state '$state'. Powering off..."
            & $VBoxManage controlvm $VmName poweroff 2>$null | Out-Null
            
            # Wait for VM to reach poweroff state (timeout: 10s)
            $timeoutSeconds = 10
            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            while ($sw.Elapsed.TotalSeconds -lt $timeoutSeconds) {
                Start-Sleep -Milliseconds 500
                $check = (& $VBoxManage showvminfo $VmName --machinereadable 2>$null) -join "`n"
                if ($check -match 'VMState="poweroff"') {
                    Write-Host "[RUN] VM '$VmName' successfully powered off."
                    break
                }
            }
            Start-Sleep -Milliseconds 1500
        }
    }
} catch {
    Write-Host "[WARN] Exception while checking VM state: $_"
}

# 3. Ensure destination directory exists
if (-not (Test-Path $DestDir)) {
    New-Item -ItemType Directory -Path $DestDir -Force | Out-Null
}

# 4. Copy ISO with retry loop to handle Windows file locks
if ($SourceIso -and (Test-Path $SourceIso)) {
    $copied = $false
    $maxAttempts = 10
    for ($i = 1; $i -le $maxAttempts; $i++) {
        try {
            Copy-Item -Path $SourceIso -Destination $DestIso -Force
            $copied = $true
            Write-Host "[RUN] Copied ISO to $DestIso (attempt $i)"
            break
        } catch {
            Write-Host "[RUN] Waiting for file lock on $DestIso (attempt $i/$maxAttempts)..."
            Start-Sleep -Milliseconds 600
        }
    }
    if (-not $copied) {
        Write-Error "[ERROR] Failed to overwrite $DestIso after $maxAttempts attempts."
        exit 1
    }
}

# 5. Start the VirtualBox VM
Write-Host "[RUN] Starting VirtualBox VM '$VmName'..."
$started = $false
for ($attempt = 1; $attempt -le 5; $attempt++) {
    & $VBoxManage startvm $VmName
    if ($LASTEXITCODE -eq 0) {
        $started = $true
        Write-Host "[RUN] VM started successfully."
        break
    }
    Write-Host "[RUN] Waiting for session unlock (attempt $attempt/5)..."
    Start-Sleep -Seconds 1
}
if (-not $started) {
    Write-Host "[ERROR] Could not start VM '$VmName'."
    exit 1
}

