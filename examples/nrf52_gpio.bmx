SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.GPIO
Import NRF52.System.Time

Const LEDPin:UInt = 15

Local ready:Int = GPIOIsValid(LEDPin) And Not GPIOIsValid(18) And Not GPIOIsValid(24) And Not GPIOIsValid(25)
ready = ready And GPIOInit(LEDPin) And GPIOSetInput(LEDPin)
Local pulls:Int = ready And GPIOPullUp(LEDPin) And GPIOIsPulledUp(LEDPin) And GPIODisablePulls(LEDPin) And Not GPIOIsPulledUp(LEDPin)
ready = ready And pulls And GPIOSetOutput(LEDPin)
Local drive:Int = ready And GPIOSetDriveStrength(LEDPin, GPIODriveStrengthMaximum) And ..
    GPIOGetDriveStrength(LEDPin) = GPIODriveStrengthMaximum And ..
    Not GPIOSetDriveStrength(LEDPin, GPIODriveStrengthMedium) And ..
    GPIOSetDriveStrength(LEDPin, GPIODriveStrengthLow) And ..
    GPIOGetDriveStrength(LEDPin) = GPIODriveStrengthLow
ready = ready And drive

While True
    Local high:Int = ready And GPIOPut(LEDPin, 1) And GPIOGetOutput(LEDPin) And GPIOGet(LEDPin)
    SleepMilliseconds(250)
    Local low:Int = ready And GPIOPut(LEDPin, 0) And Not GPIOGetOutput(LEDPin) And Not GPIOGet(LEDPin)
    If high And low Then
        Print "NRF52_GPIO,output=pass,input=pass,pulls=pass,reserved=pass,drive=pass"
    Else
        Print "NRF52_GPIO,failed"
    End If
    SleepMilliseconds(750)
Wend
