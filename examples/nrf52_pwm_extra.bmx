SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.GPIO
Import Embedded.Hardware.PWM
Import NRF52.System.Time

' Build with nrf52_pwm_extra.overlay. Connect D6 directly to D7.
' Each PWM output is checked in turn; the other pad is a GPIO input.
Const D6Pin:UInt = 32 ' P1.00, PWM2
Const D7Pin:UInt = 11 ' P0.11, PWM3

Local ready:Int = PWMIsValidPin(D6Pin) And PWMIsValidPin(D7Pin)
Local edges6:Int
Local edges7:Int
Local init6:UInt
Local init7:UInt

If ready Then ready = GPIOInit(D7Pin) And GPIOSetInput(D7Pin) And ..
    GPIOPullDown(D7Pin)
If ready Then init6 = PWMInitPin(D6Pin, 100, 32768)
If init6 Then
    Local previous:Int = GPIOGet(D7Pin)
    For Local sample:Int = 0 Until 300
        SleepMilliseconds(1)
        Local current:Int = GPIOGet(D7Pin)
        If current <> previous Then edges6 :+ 1
        previous = current
    Next
    ready = PWMDeinitPin(D6Pin) And edges6 >= 10
End If

If ready Then ready = GPIOInit(D6Pin) And GPIOSetInput(D6Pin) And ..
    GPIOPullDown(D6Pin)
If ready Then init7 = PWMInitPin(D7Pin, 160, 32768)
If init7 Then
    Local previous:Int = GPIOGet(D6Pin)
    For Local sample:Int = 0 Until 300
        SleepMilliseconds(1)
        Local current:Int = GPIOGet(D6Pin)
        If current <> previous Then edges7 :+ 1
        previous = current
    Next
    ready = PWMDeinitPin(D7Pin) And edges7 >= 10
End If

While True
    If ready And edges6 >= 10 And edges7 >= 10 Then
        Print "NRF52_PWM_EXTRA,d6=pass,d7=pass"
    Else
        Print "NRF52_PWM_EXTRA,failed,init=" + init6 + "/" + init7 + ..
            ",edges=" + edges6 + "/" + edges7 + ",setup=" + ready
    End If
    SleepMilliseconds(1000)
Wend
