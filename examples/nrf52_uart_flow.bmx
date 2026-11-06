SuperStrict

Framework BRL.StandardIO
Import BRL.EventQueue
Import Embedded.Hardware.UART
Import Embedded.IO.BufferedUART
Import NRF52.System.Time

Const Controller:Int = UARTController0
Print "NRF52_UART_FLOW,starting"
Local ready:Int = UARTConfigurePins(Controller, 9, 10) And ..
    UARTConfigureFlowControlPins(Controller, 8, 6) And ..
    UARTInit(Controller, 115200) = 115200
Local plainWrite:Int
Local plainRead:Int = -1
Local plainValue:Int = -1
If ready Then
    Local received:Byte
    plainWrite = UARTPutByte(Controller, $55)
    plainRead = UARTReadTimeout(Controller, Varptr received, 1, 100000)
    plainValue = received
    Print "NRF52_UART_FLOW,plain_write=" + plainWrite + ..
        ",plain_read=" + plainRead + ",plain_value=" + plainValue
    ready = plainWrite = 1 And plainRead = 1 And plainValue = $55
End If
If ready Then ready = Not UARTSetFlowControl(Controller, True, False) And ..
    UARTSetFlowControl(Controller, True, True)
If ready Then Print "NRF52_UART_FLOW,configured"

Local passed:Int = ready
Local failureAt:Int
Local readCount:Int = -1
Local lastReceived:Int = -1
If passed Then
    For Local value:Int = 1 Until 33
        Local received:Byte
        Local writeCount:Int = UARTPutByte(Controller, UInt(value))
        readCount = UARTReadTimeout(Controller, Varptr received, 1, 100000)
        lastReceived = received
        If writeCount <> 1 Or readCount <> 1 Or received <> value Then
            passed = False
            failureAt = value
            Exit
        End If
    Next
End If
Local buffered:Int
If passed Then
    Print "NRF52_UART_FLOW,buffered_opening"
    Local stream:TBufferedUARTStream = OpenBufferedUARTStream(Controller, 128, 128)
    buffered = stream <> Null And stream.IsOpen()
    Print "NRF52_UART_FLOW,buffered_open=" + buffered
    If buffered Then
        Local sent:Byte[96]
        Local received:Byte[96]
        For Local i:Int = 0 Until sent.Length
            sent[i] = Byte((i * 37 + 11) & $ff)
        Next
        buffered = stream.Write(sent, sent.Length) = sent.Length
        Print "NRF52_UART_FLOW,buffered_write=" + buffered
        If buffered Then stream.Flush()
        Print "NRF52_UART_FLOW,buffered_flushed"
        Local deadline:ULong = MonotonicMilliseconds() + 500
        While buffered And stream.Available() < UInt(received.Length) And ..
            MonotonicMilliseconds() < deadline
            SleepMilliseconds(1)
        Wend
        If buffered Then buffered = stream.Available() >= UInt(received.Length)
        If buffered Then buffered = stream.Read(received, received.Length) = received.Length
        If buffered Then
            For Local i:Int = 0 Until sent.Length
                If sent[i] <> received[i] Then buffered = False
            Next
        End If
        stream.Close()
    End If
End If
Local disabled:Int
Local deinitialized:Int
If ready Then
    disabled = UARTSetFlowControl(Controller, False, False)
    deinitialized = UARTDeinit(Controller)
End If

While True
    If ready And passed And buffered And disabled And deinitialized Then
        Print "NRF52_UART_FLOW,loopback=pass,rts_cts=pass,buffered=pass"
    Else
        Print "NRF52_UART_FLOW,failed,setup=" + ready + ",at=" + failureAt + ..
            ",read=" + readCount + ",received=" + lastReceived + ..
            ",buffered=" + buffered + ",disable=" + disabled + ..
            ",deinit=" + deinitialized + ..
            ",plain_read=" + plainRead
    End If
    SleepMilliseconds(1000)
Wend
