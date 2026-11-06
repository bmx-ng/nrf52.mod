SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local result:Int = BLEInitialize("NRF Lifecycle")
If result <> 0 Then RuntimeError "BLE init failed: " + result

result = BLEStartAdvertising(Null, 0, False, False)
If result <> 0 Or Not BLEAdvertisingActive() Then RuntimeError "Nonconnectable advertising failed: " + result
result = BLEStopAdvertising()
If result <> 0 Or BLEAdvertisingActive() Then RuntimeError "Stop advertising failed: " + result
result = BLEStartAdvertising(Null, 0, True, False)
If result <> 0 Or Not BLEAdvertisingActive() Then RuntimeError "Connectable advertising failed: " + result
result = BLEStopAdvertising()
If result <> 0 Or BLEAdvertisingActive() Then RuntimeError "Second stop failed: " + result
Print "BLE_LIFECYCLE,advertising=pass"

Local missing:Byte[] = [$C0, $DE, $CA, $FE, $BA, $BE]
result = BLEConnect(missing, 0, 10000)
If result <> 0 Then RuntimeError "Connect start failed: " + result
Local start:UInt = MilliSecs()
Local canceled:Int, cancelEvent:Int
While Not cancelEvent And MilliSecs() - start < 12000
    Select PollEvent()
        Case EVENT_BLECONNECTED
            If EventData() = 0 Then RuntimeError "Unexpected connection to missing peer"
            cancelEvent = True
    End Select
    If Not canceled And MilliSecs() - start > 250 Then
        result = BLECancelConnect()
        If result <> 0 Then RuntimeError "Cancel failed: " + result
        canceled = True
    End If
    SleepMilliseconds(1)
Wend
If Not canceled Or Not cancelEvent Or BLEConnectionCount() <> 0 Then RuntimeError "Cancel did not finish"
Print "BLE_LIFECYCLE,cancel=pass"

result = BLEStartScan(0, True, True)
If result <> 0 Or Not BLEScanActive() Then RuntimeError "Scan after cancel failed: " + result
result = BLEStopScan()
If result <> 0 Or BLEScanActive() Then RuntimeError "Stop scan failed: " + result
Print "BLE_LIFECYCLE,scan_after_cancel=pass"

result = BLEDeinitialize()
If result <> 0 Or BLEInitialized() Then RuntimeError "Deinit failed: " + result
result = BLEInitialize("NRF Lifecycle Again")
If result <> 0 Or Not BLEReady() Then RuntimeError "Reinit failed: " + result
result = BLEStartScan(0, False, False)
If result <> 0 Then RuntimeError "Scan after reinit failed: " + result
result = BLEStopScan()
If result <> 0 Then RuntimeError "Stop after reinit failed: " + result
Print "NRF52_BLE_LIFECYCLE,advertising=pass,cancel=pass,scan=pass,reinitialize=pass"
While True
    SleepMilliseconds(1000)
Wend
