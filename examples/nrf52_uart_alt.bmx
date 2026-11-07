SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.UART
Import NRF52.System.Time

' Build with nrf52_uart_alt.overlay and connect D6 to D7.
Print "NRF52_UART_ALT,stage=start"
Local ready:Int = UARTControllerCount() = 1 And ..
    UARTDefaultController() = UARTUnavailableController
Print "NRF52_UART_ALT,stage=configure"
If ready Then ready = UARTConfigurePins(UARTController0, 32, 11)
Print "NRF52_UART_ALT,stage=init"
If ready Then ready = UARTInit(UARTController0, 115200) = 115200
Print "NRF52_UART_ALT,stage=format"
If ready Then ready = UARTIsEnabled(UARTController0) And ..
    UARTIsWritable(UARTController0) And ..
    UARTSetFormat(UARTController0, 8, 1, UARTParityNone)
Print "NRF52_UART_ALT,stage=transfer"

Local sent:Byte[8]
Local received:Byte[8]
sent[0] = $12
sent[1] = $34
sent[2] = $56
sent[3] = $78
sent[4] = $a5
sent[5] = $5a
sent[6] = $c3
sent[7] = $3c

Local passed:Int = ready
If passed Then
    For Local i:Int = 0 Until sent.Length
        If UARTPutByte(UARTController0, sent[i]) <> 1 Then
            passed = False
            Exit
        End If
        If UARTReadTimeout(UARTController0, Varptr received[i], 1, 100000) <> 1 Then
            passed = False
            Exit
        End If
        If received[i] <> sent[i] Then
            passed = False
            Exit
        End If
    Next
End If
If ready Then ready = UARTDeinit(UARTController0)
Print "NRF52_UART_ALT,stage=done"

While True
    If Not ready Then
        Print "NRF52_UART_ALT,setup=fail"
    Else If passed Then
        Print "NRF52_UART_ALT,loopback=pass,8n1=pass"
    Else
        Print "NRF52_UART_ALT,loopback=fail"
    End If
    SleepMilliseconds(1000)
Wend
