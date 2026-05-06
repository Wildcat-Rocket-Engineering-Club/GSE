# GSE Setup Script - Auto-Elevating
if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Start-Process -FilePath PowerShell.exe -Verb RunAs -ArgumentList "-File `"$($MyInvocation.MyCommand.Path)`""
    exit
}

# 1. Folder Selection
Add-Type -AssemblyName System.Windows.Forms
$FolderBrowser = New-Object System.Windows.Forms.FolderBrowserDialog
$FolderBrowser.Description = "Select Installation Folder"
if ($FolderBrowser.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { 
    $installPath = $FolderBrowser.SelectedPath 
} else { exit }

# Set up WebClient for reliable campus downloads
$wc = New-Object System.Net.WebClient
$wc.Headers.Add("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)")
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

# 2. Interactive Download Section
Write-Host "`n--- GSE Tools Installation ---" -ForegroundColor Cyan

# VirtualHere
$vh = Read-Host "Download VirtualHere Client? (Y/N)"
if ($vh -eq "y") {
    Write-Host "Downloading VirtualHere..." -ForegroundColor Yellow
    $wc.DownloadFile("https://www.virtualhere.com/sites/default/files/usbclient/vhui64.exe", (Join-Path $installPath "VirtualHere_Client.exe"))
}

# Serial Monitor
$sm = Read-Host "Download PuTTY? (Y/N)"
if ($sm -eq "y") {
    Write-Host "Downloading PuTTY..." -ForegroundColor Yellow
    $exePath = Join-Path $installPath "putty.exe"
    $wc.DownloadFile("https://the.earth.li/~sgtatham/putty/latest/w64/putty.exe", $exePath)
}


# WinDaq
$wd = Read-Host "Download WinDaq Setup (Large File)? (Y/N)"
if ($wd -eq "y") {
    Write-Host "Downloading WinDaq (160MB)..." -ForegroundColor Yellow
    $wc.DownloadFile("https://www.dataq.com/support/downloads/DATAQ-Software-Suite-Web-Setup.exe", (Join-Path $installPath "WinDaq_Setup.exe"))
    Start-Process (Join-Path $installPath "WinDaq_Setup.exe")
}

# Network Switchers
$ns = Read-Host "Create Network Auto-Config Batch Files? (Y/N)"
if ($ns -eq "y") {
    Write-Host "Creating Batch Files..." -ForegroundColor Yellow
    "@echo off`nnetsh interface ip set address name=""Wi-Fi"" static 192.168.0.150 255.255.255.0 192.168.0.151`necho Switched to GSE Bridge Mode (.150)`npause" | Out-File (Join-Path $installPath "GO_TO_BRIDGE_MODE.bat") -Encoding ASCII
    "@echo off`nnetsh interface ip set address name=""Wi-Fi"" dhcp`nnetsh interface ip set dns name=""Wi-Fi"" dhcp`necho Back to Normal Wi-Fi (DHCP)`npause" | Out-File (Join-Path $installPath "RETURN_TO_NORMAL_WIFI.bat") -Encoding ASCII
}

Write-Host "`nSetup Complete!" -ForegroundColor Green
ii $installPath
pause
