SuperStrict

Framework BRL.StandardIO
Import NRF52.Hardware.Watchdog
Import NRF52.System.Device
Import NRF52.System.Time

Local cause:UInt = ResetCauseFlags()
If WatchdogCausedReboot() Then
    While True
        Print "NRF52_WATCHDOG,reset=pass,cause=" + cause
        SleepMilliseconds(1000)
    Wend
End If

' Arm only after a reset-pin boot. This prevents a reset loop if the expected
' watchdog flag is not visible on the following boot.
If (cause & ResetCausePin) = 0 Then
    While True
        Print "NRF52_WATCHDOG,awaiting_pin_reset,cause=" + cause
        SleepMilliseconds(1000)
    Wend
End If

If WatchdogMaximumDelayMilliseconds() < 5000 Then RuntimeError "Watchdog range failed"
If WatchdogEnable(0) Then RuntimeError "Invalid timeout accepted"
If WatchdogIsEnabled() Then RuntimeError "Watchdog unexpectedly enabled"
If Not WatchdogEnable(5000) Then RuntimeError "Watchdog start failed"
If Not WatchdogIsEnabled() Or Not WatchdogFeed() Then RuntimeError "Watchdog feed failed"
If WatchdogDisable() Then RuntimeError "nRF watchdog unexpectedly disabled"
Print "NRF52_WATCHDOG,armed=pass,cause=" + cause
SleepMilliseconds(8000)
RuntimeError "Watchdog did not reset the board"
