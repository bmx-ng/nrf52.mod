SuperStrict

Framework BRL.StandardIO
Import NRF52.System.Time
Import "nrf52_memory.c"

Extern "C"
    Function CheckMemory:Int() = "bmx_nrf52_memory_stress_test"
End Extern

Local failedStep:Int = CheckMemory()
While True
    If failedStep Then
        Print "NRF52_MEMORY,failed_step=" + failedStep
    Else
        Print "NRF52_MEMORY,fragmentation=pass,oom=pass,recovery=pass"
    End If
    SleepMilliseconds(1000)
Wend
