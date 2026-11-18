# Compila FAROL con el toolchain pspdev (en Docker) y genera FAROL.iso y EBOOT.PBP
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

# icono y fondo del menu de la consola: el Makefile los mete en el EBOOT.PBP
python tools\mkiso.py --images build
if ($LASTEXITCODE -ne 0) { throw "Fallo al generar el icono y el fondo" }

docker run --rm -v "${PSScriptRoot}:/build" -w /build pspdev/pspdev sh -c "make clean && make"
if ($LASTEXITCODE -ne 0) { throw "Fallo la compilacion" }

python tools\mkiso.py farol.prx FAROL.iso
if ($LASTEXITCODE -ne 0) { throw "Fallo la creacion del ISO" }
