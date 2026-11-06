SuperStrict

Framework BRL.StandardIO
Import NRF52.System.Device
Import NRF52.System.Power
Import NRF52.System.Time

Local cause:UInt = ResetCauseFlags()
Local invalid:Int = SystemOffUntilGPIO(18, False) = EPowerResult.InvalidArgument
Print "NRF52_SYSTEM_OFF,ready,cause=" + cause + ",validation=" + invalid

Local command:String = Input("Send OFF or GPIO over USB serial: ")
Select command
    Case "OFF"
        Print "NRF52_SYSTEM_OFF,entering=reset_or_usb"
        SleepMilliseconds(100)
        SystemOff()
    Case "GPIO"
        Print "NRF52_SYSTEM_OFF,entering=d4_low"
        SleepMilliseconds(100)
        Local result:EPowerResult = SystemOffUntilGPIO(22, False)
        Print "NRF52_SYSTEM_OFF,entry_failed=" + Int(result)
    Default
        Print "NRF52_SYSTEM_OFF,unknown_command=" + command
End Select

While True
    SleepMilliseconds(1000)
Wend
