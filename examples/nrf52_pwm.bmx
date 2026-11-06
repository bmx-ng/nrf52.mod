SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.PWM
Import NRF52.System.Time

Const LEDPin:UInt = 15
Const D4Pin:UInt = 22

Local ready:Int = PWMIsValidPin(LEDPin) And PWMIsValidPin(D4Pin) And ..
    Not PWMIsValidPin(14)
Local achieved:UInt
If ready Then achieved = PWMInitPin(LEDPin, 10, 32768)
ready = ready And achieved > 0 And PWMGetPinFrequency(LEDPin) = achieved And ..
    PWMGetPinDuty(LEDPin) = 32768 And PWMGetPinEnabled(LEDPin)
If ready Then ready = PWMInitPin(D4Pin, 100, 32768) > 0 And ..
    PWMGetPinFrequency(D4Pin) = 100 And PWMGetPinFrequency(LEDPin) = 10

If ready Then
    SleepMilliseconds(1000)
    ready = PWMSetPinDuty(LEDPin, 16384) And ..
        PWMGetPinDuty(LEDPin) = 16384 And ..
        PWMSetPinPolarity(LEDPin, True) And PWMGetPinPolarity(LEDPin)
    SleepMilliseconds(1000)
    ready = ready And PWMSetPinEnabled(LEDPin, False) And ..
        Not PWMGetPinEnabled(LEDPin) And PWMGetPinEnabled(D4Pin)
    ready = ready And PWMSetPinEnabled(LEDPin, True) And ..
        PWMSetPinFrequency(LEDPin, 20) > 0
    SleepMilliseconds(1000)
    ready = ready And PWMDeinitPin(LEDPin) And ..
        PWMGetPinEnabled(D4Pin) And PWMDeinitPin(D4Pin)
End If

While True
    If ready Then
        Print "NRF52_PWM,frequency=pass,duty=pass,polarity=pass,independent=pass"
    Else
        Print "NRF52_PWM,failed"
    End If
    SleepMilliseconds(1000)
Wend
