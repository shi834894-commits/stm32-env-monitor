[CmdletBinding()]
param(
    [string]$ProjectRoot = (Get-Location).Path,
    [string]$IocPath,
    [string]$ElfPath,
    [string]$Device,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$root = (Resolve-Path -LiteralPath $ProjectRoot).Path
$detector = Join-Path $PSScriptRoot 'detect-stm32.ps1'
$detectedJson = & $detector -ProjectRoot $root -IocPath $IocPath -ElfPath $ElfPath -Device $Device -Json
$detected = $detectedJson | ConvertFrom-Json

$vscodeDir = Join-Path $root '.vscode'
$launchPath = Join-Path $vscodeDir 'launch.json'

if ((Test-Path -LiteralPath $launchPath) -and -not $Force) {
    throw "已存在 $launchPath。为避免覆盖现有配置，请先备份，或使用 -Force 明确覆盖。"
}

New-Item -ItemType Directory -Path $vscodeDir -Force | Out-Null

$elfForLaunch = ($detected.elfPath -replace '\\', '/')

$launch = [ordered]@{
    version = '0.2.0'
    configurations = @(
        [ordered]@{
            name = 'STM32 Cortex-Debug - ST-LINK'
            type = 'cortex-debug'
            request = 'launch'
            cwd = '${workspaceFolder}'
            executable = '${workspaceFolder}/' + $elfForLaunch
            servertype = 'stlink'
            serverpath = '${env:STM32_STLINK_GDBSERVER}'
            gdbPath = '${env:STM32_GDB}'
            device = $detected.cortexDevice
            interface = 'swd'
            runToEntryPoint = 'main'
        }
        [ordered]@{
            name = 'STM32 Cortex-Debug - External ST-LINK Server'
            type = 'cortex-debug'
            request = 'launch'
            cwd = '${workspaceFolder}'
            executable = '${workspaceFolder}/' + $elfForLaunch
            servertype = 'external'
            gdbTarget = 'localhost:61234'
            gdbPath = '${env:STM32_GDB}'
            device = $detected.cortexDevice
            interface = 'swd'
            runToEntryPoint = 'main'
        }
    )
}

$launch | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $launchPath -Encoding utf8

Write-Output "已生成：$launchPath"
Write-Output "芯片型号：$($detected.cortexDevice)"
Write-Output "ELF 文件：$($detected.elfPath)"
Write-Output ''
Write-Output '请确认以下环境变量已经配置：'
Write-Output '  STM32_STLINK_GDBSERVER = ST-LINK_gdbserver.exe 的完整路径'
Write-Output '  STM32_GDB              = arm-none-eabi-gdb.exe 的完整路径'
