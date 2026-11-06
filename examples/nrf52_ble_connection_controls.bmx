SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local result:Int = BLEInitialize("NRF Connection Controls")
Print "BLE_CONTROLS,init=" + result
result = BLEStartScan(0, True, True)
Print "BLE_CONTROLS,scan=" + result
Local state:Int = 1
Local connectionHandle:Int
Local updateDeadline:ULong
While True
    Select PollEvent()
        Case EVENT_BLESCANRESULT
            If state = 1 Then
                Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
                If advertisement And advertisement.LocalName() = "ESP Notify Target" Then
                    result = BLEStopScan()
                    Print "BLE_CONTROLS,stop_scan=" + result
                    result = BLEConnectAdvertisement(advertisement)
                    Print "BLE_CONTROLS,connect_request=" + result
                    state = 2
                End If
            End If
        Case EVENT_BLECONNECTED
            If state = 2 Then
                connectionHandle = EventX()
                Print "BLE_CONTROLS,connected=" + EventData()
                SleepMilliseconds(1200)
                Local signal:Int
                result = BLEConnectionRSSI(connectionHandle, signal)
                Print "BLE_CONTROLS,rssi_status=" + result + ",rssi=" + signal
                Local phy:TBLEPHYEvent = BLEConnectionPHY(connectionHandle)
                Print "BLE_CONTROLS,phy_status=" + phy.status + ",tx=" + phy.txPHY + ",rx=" + phy.rxPHY
                Local info:TBLEConnectionInfo = BLEConnectionInfo(connectionHandle)
                Print "BLE_CONTROLS,initial_interval_us=" + info.intervalMicroseconds
                result = BLEUpdateConnectionParameters(connectionHandle, 60000, 80000, 0, 4000)
                Local attempts:Int
                While result = -13 And attempts < 10
                    SleepMilliseconds(500)
                    result = BLEUpdateConnectionParameters(connectionHandle, 60000, 80000, 0, 4000)
                    attempts :+ 1
                Wend
                Print "BLE_CONTROLS,param_request=" + result
                If result = 0 Then
                    state = 3
                    updateDeadline = MonotonicMilliseconds() + 12000
                Else
                    result = BLESetPreferredPHY(connectionHandle, BLEPHYMask2M, BLEPHYMask2M)
                    attempts = 0
                    While result = -13 And attempts < 10
                        SleepMilliseconds(500)
                        result = BLESetPreferredPHY(connectionHandle, BLEPHYMask2M, BLEPHYMask2M)
                        attempts :+ 1
                    Wend
                    Print "BLE_CONTROLS,phy_request=" + result
                    state = 4
                End If
            End If
        Case EVENT_BLECONNECTIONUPDATED
            Local update:TBLEConnectionUpdateEvent = TBLEConnectionUpdateEvent(EventExtra())
            Print "BLE_CONTROLS,updated_status=" + update.status + ..
                ",interval_us=" + update.intervalMicroseconds
            If state = 3 Then
                result = BLESetPreferredPHY(connectionHandle, BLEPHYMask2M, BLEPHYMask2M)
                Local attempts:Int
                While result = -13 And attempts < 10
                    SleepMilliseconds(500)
                    result = BLESetPreferredPHY(connectionHandle, BLEPHYMask2M, BLEPHYMask2M)
                    attempts :+ 1
                Wend
                Print "BLE_CONTROLS,phy_request=" + result
                state = 4
            End If
        Case EVENT_BLEPHYUPDATED
            Local phy:TBLEPHYEvent = TBLEPHYEvent(EventExtra())
            Print "BLE_CONTROLS,phy_updated_status=" + phy.status + ",tx=" + phy.txPHY + ",rx=" + phy.rxPHY
            If state = 4 Then
                result = BLEDisconnect(connectionHandle)
                Print "BLE_CONTROLS,disconnect_request=" + result
                state = 5
            End If
        Case EVENT_BLEDISCONNECTED
            Print "BLE_CONTROLS,disconnected=" + EventData()
            state = 6
    End Select
    If state = 3 And MonotonicMilliseconds() >= updateDeadline Then
        Local info:TBLEConnectionInfo = BLEConnectionInfo(connectionHandle)
        Print "BLE_CONTROLS,delayed_interval_us=" + info.intervalMicroseconds
        result = BLESetPreferredPHY(connectionHandle, BLEPHYMask2M, BLEPHYMask2M)
        Print "BLE_CONTROLS,delayed_phy_request=" + result
        state = 4
    End If
    If state = 6 Then SleepMilliseconds(1000) Else SleepMilliseconds(1)
Wend
