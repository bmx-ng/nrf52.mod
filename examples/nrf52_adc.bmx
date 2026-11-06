SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.ADC
Import NRF52.System.Time

Const TestPin:UInt = 2

Local ready:Int = ADCIsValidPin(TestPin) And Not ADCIsValidPin(15)
ready = ready And ADCResolutionBitsForPin(TestPin) = 12 And ..
    ADCMaximumValueForPin(TestPin) = 4095 And ADCInitPin(TestPin)

Local bounded:Int = True
For Local i:Int = 0 Until 16
    Local value:UInt
    If Not ready Or Not ADCReadRaw(TestPin, value) Or value > 4095 Then
        bounded = False
        Exit
    End If
Next

Local closed:Int = ready And ADCDeinitPin(TestPin)
Local afterClose:UInt
closed = closed And Not ADCReadRaw(TestPin, afterClose)

While True
    If ready And bounded And closed Then
        Print "NRF52_ADC,setup=pass,read=pass,bounds=pass"
    Else
        Print "NRF52_ADC,failed"
    End If
    SleepMilliseconds(1000)
Wend
