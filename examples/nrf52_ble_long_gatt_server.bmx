SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "GATT service failed: " + BLEGATTLastError()
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead | BLEGATTWrite, [Byte(78), Byte(82), Byte(70), Byte(33)], 512)
If Not characteristic Then RuntimeError "Characteristic failed: " + BLEGATTLastError()
Local result:Int = BLEInitialize("NRF Long GATT")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "Advertising failed: " + result

Local received:Int
While True
    Select PollEvent()
        Case EVENT_BLEGATTWRITE
            Local value:Byte[] = characteristic.Value()
            If Not value Or value.length <> 300 Then RuntimeError "Long write length mismatch"
            For Local index:Int = 0 Until value.length
                If value[index] <> Byte(index) Then RuntimeError "Long write value mismatch at " + index
            Next
            received = True
            Print "NRF52_BLE_LONG_GATT_SERVER,write_300=pass"
        Case EVENT_BLEDISCONNECTED
            If received Then Print "NRF52_BLE_LONG_GATT_SERVER,disconnect=pass"
    End Select
    SleepMilliseconds(1)
Wend
