SuperStrict

Framework BRL.StandardIO
Import Embedded.System.Power
Import NRF52.System.Time

Local capabilities:UInt = PowerCapabilities()
Local supported:Int = PowerSupports(PowerCapabilityIdle | PowerCapabilitySleepTimer) And ..
    Not PowerSupports(PowerCapabilitySleepInterrupt | PowerCapabilityDormantTimer)
Local validation:Int = LowPowerSleep(0) = EPowerResult.InvalidArgument And ..
    LowPowerSleepUntilInterrupt() = EPowerResult.Unavailable And ..
    DormantSleep(1) = EPowerResult.Unavailable And ..
    DormantSleepUntilGPIO(0, True, True) = EPowerResult.Unavailable And ..
    SetUnusedPinsLowLeakage() = EPowerResult.Unavailable

Local started:ULong = MonotonicMilliseconds()
PowerIdle()
Local idleElapsed:ULong = MonotonicMilliseconds() - started
started = MonotonicMilliseconds()
Local sleepResult:EPowerResult = LowPowerSleep(30)
Local sleepElapsed:ULong = MonotonicMilliseconds() - started
Local sleeping:Int = sleepResult = EPowerResult.Success And sleepElapsed >= 25

While True
    If supported And validation And sleeping And idleElapsed > 0 Then
        Print "NRF52_POWER,capabilities=pass,validation=pass,timer=pass,idle=pass"
    Else
        Print "NRF52_POWER,capabilities=" + supported + ",validation=" + validation + ..
            ",timer=" + sleeping + ",idle_ms=" + idleElapsed + ..
            ",sleep_ms=" + sleepElapsed + ",mask=" + capabilities
    End If
    SleepMilliseconds(1000)
Wend
