SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.ADC
Import NRF52.System.Time

' Connect a measured 0–3.3 V source to D19 / P0.02, with a common ground.
' The reported millivolts use the SAADC's nominal 0.6 V reference and 1/6 gain.
Const TestPin:UInt = 2
Const SampleCount:UInt = 128

Local ready:Int = ADCInitPin(TestPin)
While True
    Local total:UInt
    Local low:UInt = 4095
    Local high:UInt
    Local count:UInt
    If ready Then
        For Local i:UInt = 0 Until SampleCount
            Local value:UInt
            If Not ADCReadRaw(TestPin, value) Then Exit
            total :+ value
            If value < low Then low = value
            If value > high Then high = value
            count :+ 1
        Next
    End If
    If count = SampleCount Then
        Local average:UInt = (total + SampleCount / 2) / SampleCount
        Local millivolts:UInt = (average * 3600 + 2048) / 4096
        Print "NRF52_ADC_REFERENCE,raw=" + average + ",millivolts=" + millivolts + ..
            ",min=" + low + ",max=" + high
    Else
        Print "NRF52_ADC_REFERENCE,read=fail,count=" + count
    End If
    SleepMilliseconds(1000)
Wend
