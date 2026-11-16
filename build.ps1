# Compila FAROL con el toolchain pspdev (en Docker) y genera FAROL.iso
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

docker run --rm -v "${PSScriptRoot}:/build" -w /build pspdev/pspdev make
if ($LASTEXITCODE -ne 0) { throw "Fallo la compilacion" }

python tools\mkiso.py farol.prx FAROL.iso
if ($LASTEXITCODE -ne 0) { throw "Fallo la creacion del ISO" }
