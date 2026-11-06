SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local result:Int = BLEInitialize("NRF Notify Client")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartScan(0, True, True)
If result <> 0 Then RuntimeError "Scan failed: " + result

Local state:Int = 1
Local connectionHandle:Int
Local targetService:TBLEClientService
Local targetCharacteristic:TBLEClientCharacteristic
Local targetCCC:TBLEClientDescriptor
Local subscribed:Int, notified:Int
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
                connectionHandle = EventX()
                result = BLEDiscoverServices(connectionHandle)
                If result <> 0 Then RuntimeError "Service discovery failed: " + result
                state = 3
            End If
        Case EVENT_BLESERVICEDISCOVERED
            If state = 3 Then
                Local service:TBLEClientService = TBLEClientService(EventExtra())
                If service And (service.UUID() = "fff0" Or service.UUID() = "FFF0") Then targetService = service
            End If
        Case EVENT_BLESERVICEDISCOVERYCOMPLETE
            If state = 3 Then
                If EventData() <> 0 Or Not targetService Then RuntimeError "Service missing"
                result = targetService.DiscoverCharacteristics()
                If result <> 0 Then RuntimeError "Characteristic discovery failed: " + result
                state = 4
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERED
            If state = 4 Then
                Local characteristic:TBLEClientCharacteristic = TBLEClientCharacteristic(EventExtra())
                If characteristic And (characteristic.UUID() = "fff1" Or characteristic.UUID() = "FFF1") Then targetCharacteristic = characteristic
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERYCOMPLETE
            If state = 4 Then
                If EventData() <> 0 Or Not targetCharacteristic Or Not targetCharacteristic.CanNotify() Then RuntimeError "Notify characteristic missing"
                result = targetCharacteristic.DiscoverDescriptors()
                If result <> 0 Then RuntimeError "Descriptor discovery failed: " + result
                state = 5
            End If
        Case EVENT_BLEDESCRIPTORDISCOVERED
            If state = 5 Then
                Local descriptor:TBLEClientDescriptor = TBLEClientDescriptor(EventExtra())
                If descriptor And descriptor.UUID() = "2902" Then targetCCC = descriptor
            End If
        Case EVENT_BLEDESCRIPTORDISCOVERYCOMPLETE
            If state = 5 Then
                If EventData() <> 0 Or Not targetCCC Then RuntimeError "CCC missing"
                result = targetCCC.SetSubscription(True, False)
                If result <> 0 Then RuntimeError "Subscription failed: " + result
                state = 6
            End If
        Case EVENT_BLEWRITECOMPLETE
            If state = 6 Then
                If EventData() <> 0 Then RuntimeError "Subscription status: " + EventData()
                subscribed = True
            End If
        Case EVENT_BLENOTIFICATION
            If state = 6 Then
                Local value:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
                If Not value Or Not value.value Or value.value.length <> 4 Or ..
                        value.value[0] <> 80 Or value.value[1] <> 85 Or ..
                        value.value[2] <> 83 Or value.value[3] <> 72 Then RuntimeError "Notification mismatch"
                notified = True
            End If
        Case EVENT_BLEDISCONNECTED
            If state = 7 Then
                Print "NRF52_BLE_NOTIFY_CLIENT,subscribe=" + subscribed + ",notify=" + notified + ",disconnect=pass"
                state = 8
            Else If state > 1 And state < 7 Then
                RuntimeError "Unexpected disconnect"
            End If
    End Select
    If state = 6 And subscribed And notified Then
        result = BLEDisconnect(connectionHandle)
        If result <> 0 Then RuntimeError "Disconnect failed: " + result
        state = 7
    End If
    If state = 8 Then SleepMilliseconds(1000) Else SleepMilliseconds(1)
Wend
