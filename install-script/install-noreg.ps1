# Check if "ehppack.exe" exists in the script's directory
$ehppackExePath = Join-Path $PSScriptRoot "ehppack.exe"
if (-not (Test-Path -Path $ehppackExePath -PathType Leaf)) {
    Write-Host "Error: 'ehppack.exe' not found in the script's directory. Please put this next to ehppack.exe!"
    Exit 1
}

# Step 1: Create the "ehppack" directory in AppData
$NewDirectory = "$env:APPDATA\ehppack"
if (-not (Test-Path -Path $NewDirectory)) {
    New-Item -Path $NewDirectory -ItemType Directory | Out-Null
    Write-Host "Created directory: $NewDirectory"
} else {
    Write-Host "Directory $NewDirectory already exists."
}

# Step 2: Copy "ehppack.exe" to the new directory (overwrite silently)
Copy-Item -Force "$PSScriptRoot\ehppack.exe" "$NewDirectory\ehppack.exe"
Write-Host "Copied ehppack.exe to $NewDirectory"

# Step 3: Add the new directory to PATH if it doesn't exist
if (-not ($env:PATH -split ';' | Select-String -SimpleMatch $NewDirectory)) {
    $newPath = [System.Environment]::GetEnvironmentVariable("PATH", "User") + ";$NewDirectory"
    [System.Environment]::SetEnvironmentVariable("PATH", $newPath, "User")
    Write-Host "Added $NewDirectory to the PATH variable."
} else {
    Write-Host "$NewDirectory is already in the PATH."
}

# Function to create or update a registry key
function CreateOrUpdateRegistryKey($keyPath, $valueName, $valueData) {
    $keyExists = Test-Path -Path $keyPath
    if (-not $keyExists) {
        New-Item -Path $keyPath -Force | Out-Null
    }
    $currentValue = Get-ItemProperty -Path $keyPath -Name $valueName -ErrorAction SilentlyContinue
    if ($currentValue -eq $null -or $currentValue.$valueName -ne $valueData) {
        Set-ItemProperty -Path $keyPath -Name $valueName -Value $valueData -Force
    }
}

RefreshEnv.cmd

Write-Host "All tasks completed."
