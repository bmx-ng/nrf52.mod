SuperStrict

Framework BRL.StandardIO
Import BRL.EventQueue
Import Embedded.Hardware.UART
Import Embedded.IO.BufferedUART
Import NRF52.System.Time

Local ready:Int = UARTConfigurePins(UARTController0, 9, 10)
If ready Then ready = UARTInit(UARTController0, 115200) = 115200
Local stream:TBufferedUARTStream
If ready Then stream = OpenBufferedUARTStream(UARTController0, 128, 128)
If ready Then ready = stream <> Null And stream.IsOpen()

Local sent:Byte[96]
Local received:Byte[96]
For Local i:Int = 0 Until sent.Length
    sent[i] = Byte((i * 37 + 11) & $ff)
Next

Local passed:Int = ready
If passed Then
    passed = stream.Write(sent, sent.Length) = sent.Length
    stream.Flush()
    Local deadline:ULong = MonotonicMilliseconds() + 500
    While stream.Available() < UInt(received.Length) And MonotonicMilliseconds() < deadline
        SleepMilliseconds(1)
    Wend
    If stream.Available() < UInt(received.Length) Then
        passed = False
    Else
        passed = stream.Read(received, received.Length) = received.Length
    End If
    For Local i:Int = 0 Until sent.Length
        If sent[i] <> received[i] Then passed = False
    Next
    If stream.DroppedBytes() <> 0 Then passed = False
    stream.Close()
End If
If ready Then ready = UARTDeinit(UARTController0)

While True
    If Not ready Then
        Print "NRF52_BUFFERED_UART,setup=fail"
    Else If passed Then
        Print "NRF52_BUFFERED_UART,loopback=pass,96bytes=pass"
    Else
        Print "NRF52_BUFFERED_UART,loopback=fail"
    End If
    SleepMilliseconds(1000)
Wend
