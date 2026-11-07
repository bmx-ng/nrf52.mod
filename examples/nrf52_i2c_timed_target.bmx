SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.I2C
Import NRF52.System.Time

' Connect nRF D8/D9 to ESP32 GPIO0/GPIO1 and share ground.
Print "NRF52_I2C_TIMED_TARGET,stage=start"
Local ready:Int = I2CConfigurePins(I2CController0, ..
    I2CDefaultSDAPin(), I2CDefaultSCLPin(), True)
If ready Then ready = I2CInit(I2CController0, I2CSpeedStandard) = I2CSpeedStandard

Local sent:Byte[2]
Local received:Byte[4]
sent[0] = $11
sent[1] = $22

Local wrote:Int
Local readBack:Int
Local combined:Int
If ready Then wrote = I2CWriteTimeout(I2CController0, $42, ..
    Varptr sent[0], sent.Length, 100000) = sent.Length
If wrote Then
    SleepMilliseconds(20)
    readBack = I2CReadTimeout(I2CController0, $42, ..
        Varptr received[0], received.Length, 100000) = received.Length
    readBack = readBack And received[0] = $4e And received[1] = $52 And ..
        received[2] = $46 And received[3] = $21
End If
If readBack Then
    SleepMilliseconds(20)
    combined = I2CWriteReadTimeout(I2CController0, $42, ..
        Varptr sent[0], 1, Varptr received[0], received.Length, 100000) = received.Length
    combined = combined And received[0] = $4e And received[1] = $52 And ..
        received[2] = $46 And received[3] = $21
End If
If ready Then ready = I2CDeinit(I2CController0)

While True
    If ready And wrote And readBack And combined Then
        Print "NRF52_I2C_TIMED_TARGET,write=pass,read=pass,restart=pass"
    Else
        Print "NRF52_I2C_TIMED_TARGET,setup=" + ready + ",write=" + wrote + ..
            ",read=" + readBack + ",restart=" + combined
    End If
    SleepMilliseconds(1000)
Wend
