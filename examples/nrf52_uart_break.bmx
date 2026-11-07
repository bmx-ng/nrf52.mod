SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.GPIO
Import Embedded.Hardware.UART
Import NRF52.System.Time

' Build with nrf52_uart_alt.overlay. Connect D6 TX to D7 RX.
Const TXPin:UInt = 32
Const RXPin:UInt = 11

Print "NRF52_UART_BREAK,stage=start"
Local setup:Int = UARTConfigurePins(UARTController0, TXPin, RXPin)
If setup Then setup = UARTInit(UARTController0, 115200) = 115200
Local pendingProtected:Int
If setup Then
    Local sent:Byte = $33
    Local received:Byte
    setup = UARTWriteBlocking(UARTController0, Varptr sent, 1) = 1
    SleepMilliseconds(20)
    If setup Then pendingProtected = Not UARTSetBreak(UARTController0, True)
    If setup Then setup = UARTReadTimeout(UARTController0, ..
        Varptr received, 1, 100000) = 1 And received = sent
End If
Local entered:Int
Local lowStart:Int
Local lowEnd:Int
Local exited:Int
Local idleHigh:Int
Local loopback:Int

If setup Then entered = UARTSetBreak(UARTController0, True)
If entered Then
    setup = GPIOInit(RXPin) And GPIOSetInput(RXPin) And GPIOPullUp(RXPin)
    If setup Then
        lowStart = Not GPIOGet(RXPin)
        SleepMilliseconds(20)
        lowEnd = Not GPIOGet(RXPin)
    End If
    exited = UARTSetBreak(UARTController0, False)
    If exited Then idleHigh = GPIOGet(RXPin)
End If

If exited Then
    Local value:Byte = $5a
    Local received:Byte
    loopback = UARTWriteBlocking(UARTController0, Varptr value, 1) = 1 And ..
        UARTReadTimeout(UARTController0, Varptr received, 1, 100000) = 1 And ..
        received = value
End If
If setup Then setup = UARTDeinit(UARTController0)

While True
    Print "NRF52_UART_BREAK,setup=" + setup + ",pending=" + pendingProtected + ..
        ",entered=" + entered + ..
        ",low=" + lowStart + "/" + lowEnd + ",exited=" + exited + ..
        ",idle=" + idleHigh + ",loopback=" + loopback
    SleepMilliseconds(1000)
Wend
