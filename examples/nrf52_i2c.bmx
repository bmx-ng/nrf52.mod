SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.I2C
Import NRF52.System.Time

Local ready:Int = I2CControllerCount() = 1 And ..
    I2CDefaultController() = I2CController0
If ready Then ready = I2CConfigurePins(I2CController0, ..
    I2CDefaultSDAPin(), I2CDefaultSCLPin(), True)
If ready Then ready = I2CInit(I2CController0, I2CSpeedStandard) = I2CSpeedStandard

Local wrote:Int = False
Local readBack:Int = False
Local combined:Int = False
Local sent:Byte[2]
Local received:Byte[4]
sent[0] = $11
sent[1] = $22
If ready Then wrote = I2CWriteBlocking(I2CController0, $42, ..
    Varptr sent[0], sent.Length) = sent.Length
If wrote Then
    SleepMilliseconds(20)
    readBack = I2CReadBlocking(I2CController0, $42, ..
        Varptr received[0], received.Length) = received.Length
    readBack = readBack And received[0] = $4e And received[1] = $52 And ..
        received[2] = $46 And received[3] = $21
End If
If readBack Then
    SleepMilliseconds(20)
    combined = I2CWriteReadBlocking(I2CController0, $42, ..
        Varptr sent[0], 1, Varptr received[0], received.Length) = received.Length
    combined = combined And received[0] = $4e And received[1] = $52 And ..
        received[2] = $46 And received[3] = $21
End If
If ready Then ready = I2CDeinit(I2CController0)

While True
    If Not ready Then
        Print "NRF52_I2C,setup=fail"
    Else If wrote And readBack And combined Then
        Print "NRF52_I2C,write=pass,read=pass,restart=pass"
    Else
        Print "NRF52_I2C,write=" + wrote + ",read=" + readBack + ",restart=" + combined
    End If
    SleepMilliseconds(1000)
Wend
