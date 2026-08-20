# Robot Framework test for pico-yubikey in Renode
#
# Usage:
#   renode-test renode/pico_yubikey_test.robot

*** Settings ***
Suite Setup       Run Keywords
    ...           Setup
    ...           Configure Emulation
Suite Teardown    Teardown

*** Keywords ***
Configure Emulation
    Execute Command          mach create
    Execute Command          machine LoadPlatformDescription @platforms/boards/rp2040-pico.resc
    Execute Command          sysbus LoadELF @${CURDIR}/../build-fw/src/pico_yubikey.elf
    Create Terminal Tester   sysbus.uart0    timeout=10

*** Test Cases ***
Firmware Boots
    Wait For Line On Uart    pico-yubikey    timeout=5
