SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local result:Int = BLEInitialize("NRF Bulk Client")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartScan(0, True, True)
If result <> 0 Then RuntimeError "Scan failed: " + result
Local state:Int = 1
Local connectionHandle:Int, negotiatedMTU:Int
Local targetService:TBLEClientService
Local targetCharacteristic:TBLEClientCharacteristic
Local outgoing:Byte[] = New Byte[300]
For Local i:Int = 0 Until outgoing.length
    outgoing[i] = Byte(255 - (i & 255))
Next
While True
    Select PollEvent()
        Case EVENT_BLESCANRESULT
            If state = 1 Then
                Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
                If advertisement And advertisement.LocalName() = "ESP Bulk Target" Then
                    result = BLEStopScan()
                    If result <> 0 Then RuntimeError "Stop scan failed: " + result
                    result = BLEConnectAdvertisement(advertisement)
                    If result <> 0 Then RuntimeError "Connect failed: " + result
                    state = 2
                End If
            End If
        Case EVENT_BLECONNECTED
            If state = 2 Then
                If EventData() <> 0 Then RuntimeError "Connection failed: " + EventData()
                connectionHandle = EventX()
                result = BLEExchangeMTU(connectionHandle)
                If result <> 0 Then RuntimeError "MTU exchange failed: " + result
                state = 3
            End If
        Case EVENT_BLEMTUCHANGED
            If state = 3 Then
                negotiatedMTU = EventData()
                Print "BLE_BULK,mtu=" + negotiatedMTU
                If negotiatedMTU < 185 Then RuntimeError "MTU too small"
                result = BLEDiscoverServices(connectionHandle)
                If result <> 0 Then RuntimeError "Service discovery failed: " + result
                state = 4
            End If
        Case EVENT_BLESERVICEDISCOVERED
            If state = 4 Then
                Local service:TBLEClientService = TBLEClientService(EventExtra())
                If service And (service.UUID() = "fff0" Or service.UUID() = "FFF0") Then targetService = service
            End If
        Case EVENT_BLESERVICEDISCOVERYCOMPLETE
            If state = 4 Then
                If EventData() <> 0 Or Not targetService Then RuntimeError "Service missing"
                result = targetService.DiscoverCharacteristics()
                If result <> 0 Then RuntimeError "Characteristic discovery failed: " + result
                state = 5
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERED
            If state = 5 Then
                Local characteristic:TBLEClientCharacteristic = TBLEClientCharacteristic(EventExtra())
                If characteristic And (characteristic.UUID() = "fff1" Or characteristic.UUID() = "FFF1") Then targetCharacteristic = characteristic
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERYCOMPLETE
            If state = 5 Then
                If EventData() <> 0 Or Not targetCharacteristic Or ..
                        Not targetCharacteristic.CanRead() Or Not targetCharacteristic.CanWrite() Or ..
                        Not targetCharacteristic.CanWriteWithoutResponse() Then RuntimeError "Characteristic missing"
                result = targetCharacteristic.Read()
                If result <> 0 Then RuntimeError "Initial read failed: " + result
                state = 6
            End If
        Case EVENT_BLEREADCOMPLETE
            Local value:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
            If Not value Or EventData() <> 0 Or Not value.value Then RuntimeError "Read failed: " + EventData()
            Select state
                Case 6
                    If value.value.length <> 300 Then RuntimeError "Initial length mismatch"
                    For Local i:Int = 0 Until 300
                        If value.value[i] <> Byte(i & 255) Then RuntimeError "Initial data mismatch"
                    Next
                    Print "BLE_BULK,read_300=pass"
                    result = targetCharacteristic.Read(50)
                    If result <> 0 Then RuntimeError "Offset read failed: " + result
                    state = 7
                Case 7
                    If value.value.length <> 250 Then RuntimeError "Offset length mismatch"
                    For Local i:Int = 0 Until 250
                        If value.value[i] <> Byte((i + 50) & 255) Then RuntimeError "Offset data mismatch"
                    Next
                    Print "BLE_BULK,offset_50=pass"
                    result = targetCharacteristic.Write(outgoing, True)
                    If result <> 0 Then RuntimeError "Long write failed: " + result
                    state = 8
                Case 9
                    If value.value.length <> 300 Then RuntimeError "Readback length mismatch"
                    For Local i:Int = 0 Until 300
                        If value.value[i] <> outgoing[i] Then RuntimeError "Readback mismatch"
                    Next
                    Print "BLE_BULK,long_write_300=pass"
                    Local shortValue:Byte[] = New Byte[16]
                    For Local i:Int = 0 Until 16
                        shortValue[i] = Byte(160 + i)
                    Next
                    result = targetCharacteristic.Write(shortValue, False)
                    If result <> 0 Then RuntimeError "Write without response failed: " + result
                    state = 10
                Case 11
                    If value.value.length <> 16 Then RuntimeError "Short readback length mismatch"
                    For Local i:Int = 0 Until 16
                        If value.value[i] <> Byte(160 + i) Then RuntimeError "Short readback mismatch"
                    Next
                    Print "BLE_BULK,write_no_response_16=pass"
                    result = BLEDisconnect(connectionHandle)
                    If result <> 0 Then RuntimeError "Disconnect failed: " + result
                    state = 12
            End Select
        Case EVENT_BLEWRITECOMPLETE
            If EventData() <> 0 Then RuntimeError "Write status: " + EventData()
            If state = 8 Then
                result = targetCharacteristic.Read()
                If result <> 0 Then RuntimeError "Long readback failed: " + result
                state = 9
            Else If state = 10 Then
                result = targetCharacteristic.Read()
                If result <> 0 Then RuntimeError "Short readback failed: " + result
                state = 11
            End If
        Case EVENT_BLEDISCONNECTED
            If state = 12 Then
                Print "NRF52_BLE_BULK_CLIENT,mtu=" + negotiatedMTU + ..
                    ",read=pass,offset=pass,long_write=pass,write_no_response=pass,disconnect=pass"
                state = 13
            Else If state > 1 And state < 12 Then
                RuntimeError "Unexpected disconnect"
            End If
    End Select
    If state = 13 Then SleepMilliseconds(1000) Else SleepMilliseconds(1)
Wend
