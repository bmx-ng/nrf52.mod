SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local serviceUUIDs:String[] = ["F100", "F101", "F102", "F103"]
Local characteristicUUIDs:String[] = ["F110", "F111", "F112", "F113", ..
    "F114", "F115", "F116", "F117"]
Local services:TBLEGATTService[] = New TBLEGATTService[4]
For Local i:Int = 0 Until 4
    services[i] = CreateBLEGATTService(serviceUUIDs[i])
    If Not services[i] Then RuntimeError "Service creation failed: " + BLEGATTLastError()
    For Local j:Int = 0 Until 2
        Local characteristic:TBLEGATTCharacteristic = services[i].AddCharacteristic( ..
            characteristicUUIDs[i * 2 + j], BLEGATTRead, [Byte(i * 2 + j)], 8)
        If Not characteristic Then RuntimeError "Characteristic creation failed: " + BLEGATTLastError()
    Next
Next
If CreateBLEGATTService("F104") Then RuntimeError "Fifth service was accepted"
If BLEGATTLastError() = 0 Then RuntimeError "Missing service capacity error"
If services[0].AddCharacteristic("F118", BLEGATTRead, [Byte(8)], 8) Then ..
    RuntimeError "Ninth characteristic was accepted"
If BLEGATTLastError() = 0 Then RuntimeError "Missing characteristic capacity error"
Print "NRF52_BLE_LIMITS,services=4,characteristics=8,capacity=pass"

Local result:Int = BLEInitialize("NRF Limits")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartScan(0, True, False)
If result <> 0 Then RuntimeError "Scan failed: " + result
SleepMilliseconds(7000)
Local dropped:UInt = BLEDroppedEvents()
result = BLEStopScan()
If result <> 0 Then RuntimeError "Stop scan failed: " + result
Local scanResults:Int
Local completed:Int
Local deadline:UInt = MilliSecs() + 3000
While Not completed And MilliSecs() < deadline
    Select PollEvent()
        Case EVENT_BLESCANRESULT
            scanResults :+ 1
        Case EVENT_BLESCANCOMPLETE
            completed = True
    End Select
    SleepMilliseconds(1)
Wend
If dropped = 0 Or Not completed Or BLEScanActive() Then ..
    RuntimeError "Scan queue limit check failed"
Print "NRF52_BLE_LIMITS,scan_results=" + scanResults + ..
    ",dropped=" + dropped + ",completion=pass"
While True
    SleepMilliseconds(1000)
Wend
