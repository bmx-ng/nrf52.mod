SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local result:Int = BLEInitialize("NRF Central")
If result <> 0 Then RuntimeError "BLE init failed: " + result

Local state:Int
Local connectedAt:UInt, nextScanAt:UInt
Local connectionHandle:Int
Local cycles:Int
Local disconnectRequested:Int
While True
    If state = 0 And MilliSecs() >= nextScanAt Then
        result = BLEStartScan(0, True, True)
        If result <> 0 Then RuntimeError "Scan failed: " + result
        state = 1
    End If
    Select PollEvent()
        Case EVENT_BLESCANRESULT
            If state = 1 Then
                Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
                If advertisement And advertisement.LocalName() = "ESP Scan Target" Then
                    result = BLEStopScan()
                    If result <> 0 Then RuntimeError "Stop scan failed: " + result
                    result = BLEConnectAdvertisement(advertisement)
                    If result <> 0 Then RuntimeError "Connect failed: " + result
                    state = 2
                End If
            End If
        Case EVENT_BLECONNECTED
            If state = 2 Then
                Local event:TBLEConnectionEvent = TBLEConnectionEvent(EventExtra())
                If EventData() <> 0 Then RuntimeError "Connection status: " + EventData()
                If Not event Or event.role <> 0 Then RuntimeError "Central role missing"
                connectionHandle = EventX()
                Local info:TBLEConnectionInfo = BLEConnectionInfo(connectionHandle)
                If Not info Or info.status <> 0 Or info.role <> 0 Or info.mtu < 23 Then
                    RuntimeError "Connection info failed"
                End If
                Print "NRF52_BLE_CENTRAL,connected=pass,role=" + info.role + ..
                    ",peer=" + info.AddressString() + ",mtu=" + info.mtu
                disconnectRequested = False
                connectedAt = MilliSecs()
                state = 3
            End If
        Case EVENT_BLEDISCONNECTED
            If state = 3 Or state = 4 Then
                If EventX() <> connectionHandle Then RuntimeError "Disconnect handle mismatch"
                cycles :+ 1
                Print "NRF52_BLE_CENTRAL,disconnect=" + disconnectRequested + ",connections=" + ..
                    BLEConnectionCount() + ",cycles=" + cycles
                nextScanAt = MilliSecs() + 1000
                state = 0
            End If
    End Select
    If state = 3 And MilliSecs() - connectedAt >= 2000 Then
        result = BLEDisconnect(connectionHandle)
        If result = 0 Then disconnectRequested = True
        state = 4
    End If
    SleepMilliseconds(1)
Wend
