SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local result:Int = BLEInitialize("NRF Dual Notify Client")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartScan(0, True, True)
If result <> 0 Then RuntimeError "Scan failed: " + result

Local state:Int = 1
Local handle:Int
Local service:TBLEClientService
Local first:TBLEClientCharacteristic, second:TBLEClientCharacteristic
Local firstCCC:TBLEClientDescriptor, secondCCC:TBLEClientDescriptor
Local firstWrite:Int, secondWrite:Int, firstPush:Int, secondPush:Int, overlap:Int
While True
    Select PollEvent()
        Case EVENT_BLESCANRESULT
            If state = 1 Then
                Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
                If advertisement And advertisement.LocalName() = "ESP Notify Target" Then
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
                handle = EventX()
                result = BLEDiscoverServices(handle)
                If result <> 0 Then RuntimeError "Service discovery failed: " + result
                state = 3
            End If
        Case EVENT_BLESERVICEDISCOVERED
            If state = 3 Then
                Local found:TBLEClientService = TBLEClientService(EventExtra())
                If found And found.UUID().ToLower() = "fff0" Then service = found
            End If
        Case EVENT_BLESERVICEDISCOVERYCOMPLETE
            If state = 3 Then
                If EventData() <> 0 Or Not service Then RuntimeError "Service missing"
                result = service.DiscoverCharacteristics()
                If result <> 0 Then RuntimeError "Characteristic discovery failed: " + result
                state = 4
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERED
            If state = 4 Then
                Local found:TBLEClientCharacteristic = TBLEClientCharacteristic(EventExtra())
                If found Then
                    Select found.UUID().ToLower()
                        Case "fff1"
                            first = found
                        Case "fff3"
                            second = found
                    End Select
                End If
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERYCOMPLETE
            If state = 4 Then
                If EventData() <> 0 Or Not first Or Not second Or ..
                        Not first.CanNotify() Or Not second.CanNotify() Then RuntimeError "Characteristics missing"
                result = first.DiscoverDescriptors()
                If result <> 0 Then RuntimeError "First descriptor discovery failed: " + result
                state = 5
            End If
        Case EVENT_BLEDESCRIPTORDISCOVERED
            Local descriptor:TBLEClientDescriptor = TBLEClientDescriptor(EventExtra())
            If descriptor And descriptor.UUID() = "2902" Then
                If state = 5 Then firstCCC = descriptor
                If state = 6 Then secondCCC = descriptor
            End If
        Case EVENT_BLEDESCRIPTORDISCOVERYCOMPLETE
            If state = 5 Then
                If EventData() <> 0 Or Not firstCCC Then RuntimeError "First CCC missing"
                result = second.DiscoverDescriptors()
                If result <> 0 Then RuntimeError "Second descriptor discovery failed: " + result
                state = 6
            Else If state = 6 Then
                If EventData() <> 0 Or Not secondCCC Then RuntimeError "Second CCC missing"
                result = firstCCC.SetSubscription(True, False)
                If result <> 0 Then RuntimeError "First subscribe failed: " + result
                state = 7
            End If
        Case EVENT_BLEWRITECOMPLETE
            If EventData() <> 0 Then RuntimeError "CCC write failed: " + EventData()
            Select state
                Case 7
                    firstWrite = True
                Case 8
                    secondWrite = True
                Case 9
                    result = secondCCC.SetSubscription(False, False)
                    If result <> 0 Then RuntimeError "Second unsubscribe failed: " + result
                    state = 10
                Case 10
                    result = BLEDisconnect(handle)
                    If result <> 0 Then RuntimeError "Disconnect failed: " + result
                    state = 11
            End Select
        Case EVENT_BLENOTIFICATION
            Local update:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
            If Not update Or Not update.value Or update.value.length <> 4 Or update.indication Then
                RuntimeError "Invalid notification"
            End If
            If update.attributeHandle = first.ValueHandle() Then
                If update.value[0] = 80 And update.value[1] = 85 And ..
                        update.value[2] = 83 And update.value[3] = 72 Then
                    firstPush = True
                Else If update.value[0] = 66 And update.value[1] = 79 And ..
                        update.value[2] = 84 And update.value[3] = 72 Then
                    overlap = True
                Else
                    RuntimeError "First notification mismatch"
                End If
            Else If update.attributeHandle = second.ValueHandle() Then
                If update.value[0] <> 84 Or update.value[1] <> 87 Or ..
                        update.value[2] <> 79 Or update.value[3] <> 33 Then RuntimeError "Second notification mismatch"
                secondPush = True
            Else
                RuntimeError "Unknown notification handle"
            End If
        Case EVENT_BLEDISCONNECTED
            If state = 11 Then
                Print "NRF52_BLE_DUAL_NOTIFY,first=" + firstPush + ..
                    ",second=" + secondPush + ",overlap=" + overlap + ..
                    ",unsubscribe=pass,disconnect=pass"
                state = 12
            Else If state > 1 And state < 11 Then
                RuntimeError "Unexpected disconnect"
            End If
    End Select
    If state = 7 And firstWrite And firstPush Then
        result = secondCCC.SetSubscription(True, False)
        If result <> 0 Then RuntimeError "Second subscribe failed: " + result
        state = 8
    Else If state = 8 And secondWrite And secondPush And overlap Then
        result = firstCCC.SetSubscription(False, False)
        If result <> 0 Then RuntimeError "First unsubscribe failed: " + result
        state = 9
    End If
    If state = 12 Then SleepMilliseconds(1000) Else SleepMilliseconds(1)
Wend
