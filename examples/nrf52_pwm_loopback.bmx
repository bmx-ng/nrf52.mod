SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.GPIO
Import Embedded.Hardware.PWM
Import NRF52.System.Time

Const PWMPin:UInt = 22 ' D4, P0.22
Const InputPin:UInt = 6 ' D3, P0.06

Local ready:Int = PWMIsValidPin(PWMPin) And GPIOIsValid(InputPin) And ..
    GPIOInit(InputPin) And GPIOSetInput(InputPin)
Local transitions:Int
If ready Then
    ready = PWMInitPin(PWMPin, 100, 32768) > 0
    If ready Then
        Local previous:Int = GPIOGet(InputPin)
        For Local sample:Int = 0 Until 250
            SleepMilliseconds(1)
            Local current:Int = GPIOGet(InputPin)
            If current <> previous Then transitions :+ 1
            previous = current
        Next
        ready = PWMDeinitPin(PWMPin) And transitions >= 10
    End If
End If

While True
    If ready Then
        Print "NRF52_PWM_LOOPBACK,transitions=pass,count=" + transitions
    Else
        Print "NRF52_PWM_LOOPBACK,failed,count=" + transitions
    End If
    SleepMilliseconds(1000)
Wend
