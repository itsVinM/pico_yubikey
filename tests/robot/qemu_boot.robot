*** Settings ***
Documentation     Firmware-in-the-loop tests: pico_yubikey.elf running on
...               the QEMU RP2040 fork, exercised over emulated UART0.
Suite Setup       Start Firmware
Suite Teardown    Stop Firmware
Test Timeout      30s
Library           QemuFirmware.py

*** Test Cases ***
Firmware Boots And Prints Banner
    Banner Should Appear

Config Protocol Answers GET STATUS
    ${resp}=    Get Status Should Be Ok
    Should Be True    len($resp) > 1    msg=GET_STATUS response too short: ${resp}
