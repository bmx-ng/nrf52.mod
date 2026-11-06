SuperStrict

Framework BRL.StandardIO
Import NRF52.System.Time
Import "nrf52_ble_probe.c"

Extern "C"
    Function StartBLEProbe:Int() = "bmx_nrf52_ble_probe_start"
End Extern

Local result:Int = StartBLEProbe()
While True
    If result = 0 Then
        Print "NRF52_BLE_PROBE,advertising=pass"
    Else
        Print "NRF52_BLE_PROBE,advertising=fail,error=" + result
    End If
    SleepMilliseconds(1000)
Wend
