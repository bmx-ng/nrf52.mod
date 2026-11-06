SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "Service failed: " + BLEGATTLastError()
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead, [Byte(79), Byte(75)], 16)
If Not characteristic Then RuntimeError "Characteristic failed: " + BLEGATTLastError()
Local result:Int = BLEInitialize("NRF Adv Service")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartAdvertising(service, 350, False, False)
If result <> 0 Then RuntimeError "Timed advertising failed: " + result
Local started:UInt = MilliSecs()
Local completed:Int
While Not completed And MilliSecs() - started < 3000
    Select PollEvent()
        Case EVENT_BLEADVERTISINGCOMPLETE
            If EventData() <> 0 Then RuntimeError "Advertising completion status: " + EventData()
            completed = True
    End Select
    SleepMilliseconds(1)
Wend
If Not completed Or BLEAdvertisingActive() Then RuntimeError "Timed advertising did not stop"
Print "BLE_ADV_SERVICE,timed=pass"
result = BLEStartAdvertising(service, 0, False, False)
If result <> 0 Or Not BLEAdvertisingActive() Then RuntimeError "Service advertising failed: " + result
Print "NRF52_BLE_ADV_SERVICE,timed=pass,advertising=pass"
While True
    SleepMilliseconds(1000)
Wend
