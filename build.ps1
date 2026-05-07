$qtDir = "D:\Qt"
$qtVersion = "6.10.2"
$qtArch   = "mingw_64"

$cmake    = "$qtDir\Tools\CMake_64\bin\cmake.exe"
$mingwBin = "$qtDir\Tools\mingw1310_64\bin"
$ninja    = "$qtDir\Tools\Ninja"
$qtPrefix = "$qtDir\$qtVersion\$qtArch"
$windeployqt = "$qtPrefix\bin\windeployqt6.exe"

$env:Path = "$mingwBin;$ninja;$qtDir\Tools\CMake_64\bin;$env:Path"

# ---- Build ---------------------------------------------------
if (-not (Test-Path "build/CMakeCache.txt")) {
    Write-Output "== Configuring =="
    & $cmake -B build -G Ninja `
        -DCMAKE_C_COMPILER=gcc `
        -DCMAKE_CXX_COMPILER=g++ `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_PREFIX_PATH="$qtPrefix" `
        -DWITH_GUI=ON
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Output "== Building =="
& $cmake --build build
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) { exit $exitCode }

# ---- Dist ----------------------------------------------------
Write-Output "== Organizing dist =="

# Stop any running GUI instance so file copies don't fail
Get-Process w3x-gui -ErrorAction SilentlyContinue | Stop-Process -Force

$dist = "dist"
$guiDir   = "$dist\GUI"
$exeDir   = "$dist\EXE"
$testDir  = "$dist\TEST"
$plugDir  = "$guiDir\plugins"

Remove-Item -Recurse -Force $dist -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $guiDir    | Out-Null
New-Item -ItemType Directory -Force -Path $exeDir    | Out-Null
New-Item -ItemType Directory -Force -Path $testDir   | Out-Null
New-Item -ItemType Directory -Force -Path $plugDir   | Out-Null

# DLLs -- libStormLib.dll from StormLib shared library
Copy-Item "build\libStormLib.dll" "$exeDir\"
Copy-Item "build\libStormLib.dll" "$testDir\"

# GUI -- exe + windeployqt (DLLs, plugins, translations)
Copy-Item "build\gui\w3x-gui.exe" "$guiDir\"
Copy-Item "build\gui\libStormLib.dll" "$guiDir\"
& $windeployqt --release --compiler-runtime "$guiDir\w3x-gui.exe" *>&1 | Out-Null
Copy-Item "build/gui/translations/w3x-packer_zh_CN.qm" "$guiDir/translations/"

# CLI
Copy-Item "build\w3x_packer.exe" "$exeDir\"

# Tests
Copy-Item "build\w3x_tests.exe" "$testDir\"

# Plugin DLLs
if (Test-Path "build/plugins") {
    Get-ChildItem "build/plugins/*.dll" | ForEach-Object {
        Copy-Item $_.FullName "$plugDir\"
        Write-Output "  [plugin] $($_.Name)"
    }
}

Write-Output "Done:"
Write-Output "  $guiDir\    (GUI -- w3x-gui.exe + Qt DLLs)"
Write-Output "  $exeDir\    (CLI -- w3x_packer.exe)"
Write-Output "  $testDir\   (Tests -- w3x_tests.exe)"
Write-Output "  $plugDir\   (Plugins)"
Write-Output "`nExit code: 0"
exit 0
