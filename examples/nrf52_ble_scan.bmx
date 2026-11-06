SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local result:Int = BLEInitialize("NRF Scanner")
If result <> 0 Then RuntimeError "BLE init failed: " + result

Local passCount:Int
While True
    Local phase:Int = passCount Mod 3
    Local active:Int = phase <> 1
    Local filtered:Int = phase <> 2
    Local duration:UInt = 5000
    If phase = 2 Then duration = 0
    Local sawTarget:Int, sawResponse:Int, targetCount:Int, completed:Int, stopped:Int
    Local totalResults:Int
    result = BLEStartScan(duration, active, filtered)
    If result <> 0 Then RuntimeError "BLE scan failed: " + result
    Local started:UInt = MilliSecs()
    While Not completed
        If phase = 2 And Not stopped And MilliSecs() - started >= 2000 Then
            result = BLEStopScan()
            If result <> 0 Then RuntimeError "BLE stop failed: " + result
            stopped = True
        End If
        Select PollEvent()
            Case EVENT_BLESCANRESULT
                totalResults :+ 1
                Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
                If advertisement And advertisement.LocalName() = "ESP Scan Target" Then
                    sawTarget = True
                    If advertisement.IsScanResponse() Then sawResponse = True
                    targetCount :+ 1
                    If targetCount = 1 Then
                        Print "NRF52_BLE_SCAN,result=target,type=" + advertisement.eventType + ..
                            ",rssi=" + advertisement.rssi + ",name=" + advertisement.LocalName()
                    End If
                End If
            Case EVENT_BLESCANCOMPLETE
                completed = True
        End Select
        SleepMilliseconds(1)
    Wend
    Print "NRF52_BLE_SCAN,active=" + active + ",filtered=" + filtered + ..
        ",stopped=" + stopped + ",target=" + sawTarget + ..
        ",response=" + sawResponse + ",count=" + targetCount + ..
        ",total=" + totalResults + ",complete=" + completed + ",scanning=" + BLEScanActive() + ..
        ",dropped=" + BLEDroppedEvents()
    passCount :+ 1
    SleepMilliseconds(1000)
Wend
