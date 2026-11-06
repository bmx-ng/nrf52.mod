SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.SPI
Import NRF52.System.Time

Local controller:Int = SPIDefaultController()
Local ready:Int = SPIControllerCount() = 1 And controller = SPIController0
ready = ready And SPIDefaultRXPin() = 39 And ..
    SPIDefaultTXPin() = 34 And SPIDefaultClockPin() = 33
ready = ready And SPIDefaultChipSelectPin() = SPIUnavailablePin
If ready Then ready = SPIConfigurePins(controller, 39, 34, 33)
If ready Then ready = SPIInit(controller, 1000000) > 0
If ready Then ready = SPIGetBaudrate(controller) > 0 And ..
    SPISetFormat(controller, 8, SPIClockPolarity0, SPIClockPhase0, SPIBitOrderMSBFirst)

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
If passed Then passed = SPIWriteReadBlocking(controller, Varptr sent[0], ..
    Varptr received[0], sent.Length) = sent.Length
If passed Then
    For Local i:Int = 0 Until sent.Length
        If received[i] <> sent[i] Then passed = False
    Next
End If
If passed Then passed = SPISetBaudrate(controller, 750000) = 500000 And ..
    SPISetFormat(controller, 8, SPIClockPolarity1, SPIClockPhase1, SPIBitOrderLSBFirst)
If passed Then passed = SPIReadBlocking(controller, $a5, Varptr received[0], ..
    received.Length) = received.Length
If passed Then
    For Local i:Int = 0 Until received.Length
        If received[i] <> $a5 Then passed = False
    Next
End If

Local sent16:Short[4]
Local received16:Short[4]
sent16[0] = $1234
sent16[1] = $5678
sent16[2] = $a55a
sent16[3] = $c33c
If passed Then passed = SPISetFormat(controller, 16, SPIClockPolarity0, ..
    SPIClockPhase0, SPIBitOrderMSBFirst)
If passed Then passed = SPIWrite16Read16Blocking(controller, ..
    Varptr sent16[0], Varptr received16[0], sent16.Length) = sent16.Length
If passed Then
    For Local i:Int = 0 Until sent16.Length
        If received16[i] <> sent16[i] Then passed = False
    Next
End If
If ready Then ready = SPIDeinit(controller)

While True
    If Not ready Then
        Print "NRF52_SPI,setup=fail"
    Else If passed Then
        Print "NRF52_SPI,loopback=pass,8bit=pass,16bit=pass,mode3=pass"
    Else
        Print "NRF52_SPI,loopback=fail"
    End If
    SleepMilliseconds(1000)
Wend
