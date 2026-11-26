# Builds and runs the host-side unit tests (needs a C++17 compiler: MSVC via vcvars64.bat, or g++/clang on PATH).
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $out = Join-Path $env:TEMP 'genius-native-tests'
    New-Item -ItemType Directory -Force $out | Out-Null
    $failed = $false
    foreach ($name in @('test_tree')) {
        if (Get-Command g++ -ErrorAction SilentlyContinue) {
            g++ -std=c++17 -Wall -Wextra -I ../../src "$name.cpp" -o "$out/$name.exe"
        } else {
            $vc = Get-ChildItem "C:\Program Files*\Microsoft Visual Studio\*\*\VC\Auxiliary\Build\vcvars64.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
            if (-not $vc) { throw 'No C++ compiler found (install MSVC build tools or g++).' }
            cmd /c "`"$($vc.FullName)`" >nul && cl /nologo /std:c++17 /EHsc /W3 /I ..\..\src /Fo:`"$out\\`" /Fe:`"$out\$name.exe`" $name.cpp"
        }
        if ($LASTEXITCODE -ne 0) { throw "Build of $name failed." }
        Write-Host "== $name"
        & "$out/$name.exe"
        if ($LASTEXITCODE -ne 0) { $failed = $true }
    }
    exit ([int]$failed)
} finally { Pop-Location }
