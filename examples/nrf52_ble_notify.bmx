SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local initial:Byte[] = [Byte(78), Byte(82), Byte(70), Byte(33)]
Local pushed:Byte[] = [Byte(80), Byte(85), Byte(83), Byte(72)]
If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "GATT service failed: " + BLEGATTLastError()
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead | BLEGATTNotify, initial, 64)
If Not characteristic Then RuntimeError "GATT characteristic failed: " + BLEGATTLastError()

Local result:Int = BLEInitialize("NRF Notify")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "BLE advertising failed: " + result

Local subscribed:Int, notified:Int
While True
    Select PollEvent()
        Case EVENT_BLECONNECTED
            subscribed = False
            notified = False
        Case EVENT_BLEDISCONNECTED
            subscribed = False
            notified = False
            characteristic.SetValue(initial)
        Case EVENT_BLEGATTSUBSCRIBE
            subscribed = EventData() <> 0
            If subscribed Then
                result = characteristic.SetValue(pushed, True)
                If result <> 0 Then RuntimeError "Notify failed: " + result
            End If
        Case EVENT_BLEGATTUPDATECOMPLETE
            notified = True
    End Select
    If notified Then
        Print "NRF52_BLE_NOTIFY,subscribe=pass,notify=pass"
    Else If subscribed Then
        Print "NRF52_BLE_NOTIFY,subscribe=pass,waiting=notification"
    Else
        Print "NRF52_BLE_NOTIFY,advertising=pass"
    End If
    SleepMilliseconds(1000)
Wend
