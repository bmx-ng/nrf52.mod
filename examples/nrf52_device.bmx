SuperStrict

Framework BRL.StandardIO
Import NRF52.System.Device
Import NRF52.System.Time

Local identifier:String = UniqueDeviceID()
Local bytes:Byte[] = UniqueDeviceIDBytes()
Local supported:UInt = SupportedResetCauseFlags()
Local cause:UInt = ResetCauseFlags()
Local passed:Int = identifier.length = 16 And bytes.length = 8 And ..
    identifier = UniqueDeviceID() And supported <> 0

While True
    If passed Then
        Print "NRF52_DEVICE,id=pass,reset_api=pass,cause=" + cause
    Else
        Print "NRF52_DEVICE,failed"
    End If
    SleepMilliseconds(1000)
Wend
