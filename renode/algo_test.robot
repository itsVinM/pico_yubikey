# Robot Framework tests for crypto algorithms on emulated RP2040
#
# The algo_test firmware reads commands on UART and prints results.
# These tests send RFC test vectors and verify the output.
#
# Usage:
#   renode-test renode/algo_test.robot

*** Settings ***
Suite Setup       Run Keywords
    ...           Setup
    ...           Configure Emulation
Suite Teardown    Teardown

*** Keywords ***
Configure Emulation
    Execute Command          mach create
    Execute Command          machine LoadPlatformDescription @platforms/boards/rp2040-pico.resc
    Execute Command          sysbus LoadELF @${CURDIR}/../build-fw/tests/rp2040/algo_test.elf
    Create Terminal Tester   sysbus.uart0    timeout=10

Send And Expect
    [Arguments]    ${cmd}    ${expected}
    Write Line To Uart    ${cmd}
    Wait For Line On Uart    ${expected}    timeout=5

*** Test Cases ***
Firmware Ready
    Wait For Line On Uart    READY    timeout=5

SHA1 Empty
    Send And Expect    SHA1:    SHA1:da39a3ee5e6b4b0d3255bfef95601890afd80709

SHA1 ABC
    Send And Expect    SHA1:616263    SHA1:a9993e364706816aba3e25717850c26c9cd0d89d

HMAC RFC2202 Case 1
    # key = 0x0b * 20, msg = "Hi There"
    Send And Expect    HMAC:0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b,4869205468657265    HMAC:b617318655057264e28bc0b6fb378c8ef146be00

HMAC RFC2202 Case 2
    # key = "Jefe", msg = "what do ya want for nothing?"
    Send And Expect    HMAC:4a656665,7768617420646f2079612077616e7420666f72206e6f7468696e673f    HMAC:effcdf6ae5eb2fa2d27416d5f184df9c259a7c79

HOTP RFC4226 Vector 0
    # key = ASCII "12345678901234567890", counter = 0, 6 digits
    Send And Expect    HOTP:3132333435363738393031323334353637383930,0,6    HOTP:755224

HOTP RFC4226 Vector 1
    Send And Expect    HOTP:3132333435363738393031323334353637383930,1,6    HOTP:287082

HOTP RFC4226 Vector 5
    Send And Expect    HOTP:3132333435363738393031323334353637383930,5,6    HOTP:254676

HOTP 8 Digits
    Send And Expect    HOTP:3132333435363738393031323334353637383930,0,8    HOTP:84755224

TOTP RFC6238 Vector t=59
    # key = ASCII "12345678901234567890", time=59, period=30, 8 digits
    Send And Expect    TOTP:3132333435363738393031323334353637383930,59,30    TOTP:94287082

TOTP RFC6238 Vector t=1111111109
    Send And Expect    TOTP:3132333435363738393031323334353637383930,1111111109,30    TOTP:7081804

TOTP RFC6238 Vector t=2000000000
    Send And Expect    TOTP:3132333435363738393031323334353637383930,2000000000,30    TOTP:69279037
