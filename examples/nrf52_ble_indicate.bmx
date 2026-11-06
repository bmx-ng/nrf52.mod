SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local initial:Byte[] = [Byte(78), Byte(82), Byte(70), Byte(33)]
Local pushed:Byte[] = [Byte(65), Byte(67), Byte(75), Byte(33)]
If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "GATT service failed: " + BLEGATTLastError()
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead | BLEGATTIndicate, initial, 64)
If Not characteristic Then RuntimeError "GATT characteristic failed: " + BLEGATTLastError()

Local result:Int = BLEInitialize("NRF Indicate")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "BLE advertising failed: " + result

Local subscribed:Int, confirmed:Int
While True
    Select PollEvent()
        Case EVENT_BLECONNECTED
            subscribed = False
            confirmed = False
        Case EVENT_BLEDISCONNECTED
            subscribed = False
            confirmed = False
            characteristic.SetValue(initial)
        Case EVENT_BLEGATTSUBSCRIBE
            subscribed = EventMods() <> 0
            If subscribed Then
                result = characteristic.SetValue(pushed)
                If result <> 0 Then RuntimeError "Set value failed: " + result
                result = characteristic.Indicate(EventX())
                If result <> 0 Then RuntimeError "Indicate failed: " + result
            End If
        Case EVENT_BLEGATTUPDATECOMPLETE
            confirmed = EventData() = 0 And EventMods() <> 0
    End Select
    If confirmed Then
        Print "NRF52_BLE_INDICATE,subscribe=pass,confirm=pass"
    Else If subscribed Then
        Print "NRF52_BLE_INDICATE,subscribe=pass,waiting=confirmation"
    Else
        Print "NRF52_BLE_INDICATE,advertising=pass"
    End If
    SleepMilliseconds(1000)
Wend
