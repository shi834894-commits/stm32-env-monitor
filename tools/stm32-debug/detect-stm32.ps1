[CmdletBinding()]
param(
    [string]$ProjectRoot = (Get-Location).Path,
    [string]$IocPath,
    [string]$ElfPath,
    [string]$Device,
    [switch]$Json
)

$ErrorActionPreference = 'Stop'

function Resolve-ProjectPath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$Path
    )

    if ([IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path -LiteralPath $Path).Path
    }

    return (Resolve-Path -LiteralPath (Join-Path $BasePath $Path)).Path
}

function Get-RelativeProjectPath {
    param(
        [Parameter(Mandatory = $true)][string]$BasePath,
        [Parameter(Mandatory = $true)][string]$Path
    )

    $baseUri = [Uri]((Resolve-Path -LiteralPath $BasePath).Path + [IO.Path]::DirectorySeparatorChar)
    $pathUri = [Uri](Resolve-Path -LiteralPath $Path).Path
    return ([Uri]::UnescapeDataString($baseUri.MakeRelativeUri($pathUri).ToString()) -replace '/', '\')
}

function Convert-ToCortexDeviceName {
    param([Parameter(Mandatory = $true)][string]$CubeDevice)

    # CubeMX commonly appends the package/temperature suffix "Tx", for example
    # STM32F103C8Tx. Cortex-Debug uses STM32F103C8 for this target.
    return ($CubeDevice.Trim() -replace '(?i)Tx$', '')
}

$root = (Resolve-Path -LiteralPath $ProjectRoot).Path

if ($IocPath) {
    $iocFullPath = Resolve-ProjectPath -BasePath $root -Path $IocPath
} else {
    $iocCandidates = @(
        Get-ChildItem -LiteralPath $root -Filter '*.ioc' -File -ErrorAction SilentlyContinue
        Get-ChildItem -LiteralPath $root -Filter '*.ioc' -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -notmatch '\\(\.git|build|Drivers|Middlewares)\\' }
    ) | Sort-Object FullName -Unique

    if ($iocCandidates.Count -eq 1) {
        $iocFullPath = $iocCandidates[0].FullName
    } elseif ($iocCandidates.Count -gt 1) {
        throw "发现多个 .ioc 文件，请使用 -IocPath 指定：$($iocCandidates.FullName -join ', ')"
    }
}

$cubeDevice = $null
if ($iocFullPath) {
    $iocText = Get-Content -LiteralPath $iocFullPath -Raw
    $deviceMatch = [regex]::Match($iocText, '(?m)^ProjectManager\.DeviceId=(.+)$')
    if ($deviceMatch.Success) {
        $cubeDevice = $deviceMatch.Groups[1].Value.Trim()
    }
}

$cortexDevice = if ($Device) {
    $Device.Trim()
} elseif ($cubeDevice) {
    Convert-ToCortexDeviceName -CubeDevice $cubeDevice
} else {
    $null
}

if (-not $cortexDevice) {
    throw '无法从 .ioc 文件识别芯片型号，请使用 -Device 指定，例如 STM32F103C8。'
}

if ($ElfPath) {
    $elfFullPath = Resolve-ProjectPath -BasePath $root -Path $ElfPath
} else {
    $elfCandidates = @(
        Get-ChildItem -LiteralPath $root -Filter '*.elf' -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -notmatch '\\(\.git|tools)\\' }
    )

    if ($elfCandidates.Count -eq 0) {
        throw '没有找到 ELF 文件，请先编译工程，或使用 -ElfPath 指定 ELF 路径。'
    }

    $elfFullPath = $elfCandidates |
        Sort-Object @(
            @{ Expression = {
                    if ($_.FullName -match '(?i)\\build\\Debug\\') { 0 }
                    elseif ($_.FullName -match '(?i)\\Debug\\') { 1 }
                    elseif ($_.FullName -match '(?i)\\build\\Release\\') { 2 }
                    elseif ($_.FullName -match '(?i)\\Release\\') { 3 }
                    else { 4 }
                }; Descending = $false },
            @{ Expression = { $_.LastWriteTime }; Descending = $true }
        ) |
        Select-Object -First 1 -ExpandProperty FullName
}

$result = [ordered]@{
    projectRoot      = $root
    iocPath          = if ($iocFullPath) { Get-RelativeProjectPath -BasePath $root -Path $iocFullPath } else { $null }
    cubeDeviceId     = $cubeDevice
    cortexDevice     = $cortexDevice
    elfPath          = Get-RelativeProjectPath -BasePath $root -Path $elfFullPath
    elfFullPath      = $elfFullPath
}

if ($Json) {
    $result | ConvertTo-Json -Depth 4
} else {
    Write-Output "工程目录 : $($result.projectRoot)"
    Write-Output "IOC 文件 : $($result.iocPath)"
    Write-Output "CubeMX 型号: $($result.cubeDeviceId)"
    Write-Output "调试型号 : $($result.cortexDevice)"
    Write-Output "ELF 文件 : $($result.elfPath)"
}
