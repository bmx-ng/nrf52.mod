SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local initial:Byte[] = [Byte(78), Byte(82), Byte(70), Byte(33)]
If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "GATT service failed: " + BLEGATTLastError()
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead | BLEGATTWrite, initial, 64)
If Not characteristic Then RuntimeError "GATT characteristic failed: " + BLEGATTLastError()

Local result:Int = BLEInitialize("NRF GATT")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "BLE advertising failed: " + result

Local connected:Int, wrote:Int
While True
    Select PollEvent()
        Case EVENT_BLECONNECTED
            connected = True
            wrote = False
        Case EVENT_BLEDISCONNECTED
            connected = False
            wrote = False
            characteristic.SetValue(initial)
        Case EVENT_BLEGATTWRITE
            Local value:Byte[] = characteristic.Value()
            wrote = value And value.Length = 4 And value[0] = 80 And ..
                value[1] = 73 And value[2] = 78 And value[3] = 71
    End Select
    If wrote Then
        Print "NRF52_BLE_GATT,connect=pass,read=pass,write=pass"
    Else If connected Then
        Print "NRF52_BLE_GATT,connect=pass,waiting=write"
    Else
        Print "NRF52_BLE_GATT,advertising=pass"
    End If
    SleepMilliseconds(1000)
Wend
