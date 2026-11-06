SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local first:TBLEGATTService = CreateBLEGATTService("FFF0")
Local second:TBLEGATTService = CreateBLEGATTService("FFE0")
If Not first Or Not second Then RuntimeError "Service creation failed: " + BLEGATTLastError()

Local one:TBLEGATTCharacteristic = first.AddCharacteristic("FFF1", BLEGATTRead, ..
    [Byte(79), Byte(78), Byte(69)], 16)
Local two:TBLEGATTCharacteristic = first.AddCharacteristic("FFF2", ..
    BLEGATTRead | BLEGATTWrite, [Byte(84), Byte(87), Byte(79)], 16)
Local three:TBLEGATTCharacteristic = second.AddCharacteristic("FFE1", BLEGATTRead, ..
    [Byte(84), Byte(72), Byte(82)], 16)
If Not one Or Not two Or Not three Then RuntimeError "Characteristic creation failed: " + BLEGATTLastError()
If one.Identifier() = two.Identifier() Or two.Identifier() = three.Identifier() Then ..
    RuntimeError "Characteristic IDs are not unique"

Local result:Int = BLEInitialize("NRF Multi GATT")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartAdvertising(first)
If result <> 0 Then RuntimeError "Advertising failed: " + result
Print "NRF52_BLE_MULTI_GATT,advertising=pass"

While True
    Select PollEvent()
        Case EVENT_BLECONNECTED
            Print "NRF52_BLE_MULTI_GATT,connected=pass"
        Case EVENT_BLEGATTWRITE
            Local changed:TBLEGATTWriteEvent = TBLEGATTWriteEvent(EventExtra())
            If Not changed Or changed.characteristic <> two Then RuntimeError "Wrong write characteristic"
            Local value:Byte[] = two.Value()
            If Not value Or value.length <> 3 Or value[0] <> 85 Or ..
                    value[1] <> 80 Or value[2] <> 33 Then RuntimeError "Wrong write value"
            Print "NRF52_BLE_MULTI_GATT,write_second=pass"
        Case EVENT_BLEDISCONNECTED
            Print "NRF52_BLE_MULTI_GATT,disconnected=pass"
    End Select
    SleepMilliseconds(1)
Wend
