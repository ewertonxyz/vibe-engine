<#
.SYNOPSIS
    Roda a suite de testes (ctest) para um preset CMake.
.DESCRIPTION
    Wrapper fino sobre `ctest --preset <preset> --output-on-failure`.
    Propaga o exit code do ctest para o chamador (gate de CI / vx-task-execute).
    Fonte das convencoes: ADR 0008, Fase-01 (Comandos canonicos).
.PARAMETER Preset
    Nome do preset CMake (debug | development | shipping | asan-debug). Default: debug.
.EXAMPLE
    Scripts/run-tests.ps1 -Preset debug
#>
param(
    [string]$Preset = "debug"
)

ctest --preset $Preset --output-on-failure
exit $LASTEXITCODE
