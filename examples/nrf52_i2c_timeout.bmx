SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.I2C
Import NRF52.System.Time
Import "nrf52_i2c_timeout.c"

Extern "C"
    Function HoldClock:Int(milliseconds:UInt) = "bmx_nrf52_i2c_test_hold_clock"
End Extern

' Jumper D19 to D9. No I2C target is needed at address $42.
Print "NRF52_I2C_TIMEOUT,stage=start"
Local ready:Int = I2CConfigurePins(I2CController0, ..
    I2CDefaultSDAPin(), I2CDefaultSCLPin(), True)
If ready Then ready = I2CInit(I2CController0, I2CSpeedStandard) = I2CSpeedStandard

Local value:Byte = $11
Local shortResult:Int = I2CErrorInvalidArgument
Local longResult:Int = I2CErrorInvalidArgument
Local shortElapsed:ULong
Local longElapsed:ULong

If ready And HoldClock(40) Then
    Local start:ULong = MonotonicMicroseconds()
    shortResult = I2CWriteTimeout(I2CController0, $42, Varptr value, 1, 5000)
    shortElapsed = MonotonicMicroseconds() - start
End If
SleepMilliseconds(80)
If ready And HoldClock(40) Then
    Local start:ULong = MonotonicMicroseconds()
    longResult = I2CWriteTimeout(I2CController0, $42, Varptr value, 1, 100000)
    longElapsed = MonotonicMicroseconds() - start
End If
SleepMilliseconds(80)
If ready Then ready = I2CDeinit(I2CController0)

While True
    Print "NRF52_I2C_TIMEOUT,short=" + shortResult + ",short_us=" + shortElapsed + ..
        ",long=" + longResult + ",long_us=" + longElapsed + ",setup=" + ready
    SleepMilliseconds(1000)
Wend
