SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

Local result:Int = BLEInitialize("NRF GATT Client")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartScan(0, True, True)
If result <> 0 Then RuntimeError "Scan failed: " + result

Local state:Int = 1
Local connectionHandle:Int
Local targetService:TBLEClientService
Local targetCharacteristic:TBLEClientCharacteristic
Local targetDescriptor:TBLEClientDescriptor
Local readInitial:Int, wrote:Int, readBack:Int, descriptorRead:Int
While True
    Select PollEvent()
        Case EVENT_BLESCANRESULT
            If state = 1 Then
                Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
                If advertisement And advertisement.LocalName() = "ESP GATT Target" Then
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
                If service And (service.UUID() = "fff0" Or service.UUID() = "FFF0") Then
                    targetService = service
                End If
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
                If characteristic And (characteristic.UUID() = "fff1" Or ..
                        characteristic.UUID() = "FFF1") Then
                    targetCharacteristic = characteristic
                End If
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERYCOMPLETE
            If state = 4 Then
                If EventData() <> 0 Or Not targetCharacteristic Or ..
                        Not targetCharacteristic.CanRead() Or Not targetCharacteristic.CanWrite() Then
                    RuntimeError "Characteristic missing"
                End If
                result = targetCharacteristic.DiscoverDescriptors()
                If result <> 0 Then RuntimeError "Descriptor discovery failed: " + result
                state = 5
            End If
        Case EVENT_BLEDESCRIPTORDISCOVERED
            If state = 5 Then
                Local descriptor:TBLEClientDescriptor = TBLEClientDescriptor(EventExtra())
                If descriptor And (descriptor.UUID() = "fff2" Or descriptor.UUID() = "FFF2") Then
                    targetDescriptor = descriptor
                End If
            End If
        Case EVENT_BLEDESCRIPTORDISCOVERYCOMPLETE
            If state = 5 Then
                If EventData() <> 0 Or Not targetDescriptor Then RuntimeError "Descriptor missing"
                result = targetDescriptor.Read()
                If result <> 0 Then RuntimeError "Descriptor read failed: " + result
                state = 6
            End If
        Case EVENT_BLEREADCOMPLETE
            Local value:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
            If Not value Or EventData() <> 0 Then RuntimeError "Read failed: " + EventData()
            If state = 6 Then
                descriptorRead = value.value And value.value.length = 3 And ..
                    value.value[0] = 68 And value.value[1] = 83 And value.value[2] = 67
                If Not descriptorRead Then RuntimeError "Descriptor value mismatch"
                result = targetCharacteristic.Read()
                If result <> 0 Then RuntimeError "Initial read failed: " + result
                state = 7
            Else If state = 7 Then
                readInitial = value.value And value.value.length = 4 And ..
                    value.value[0] = 69 And value.value[1] = 83 And ..
                    value.value[2] = 80 And value.value[3] = 33
                If Not readInitial Then RuntimeError "Initial value mismatch"
                result = targetCharacteristic.Write([Byte(80), Byte(73), Byte(78), Byte(71)])
                If result <> 0 Then RuntimeError "Write failed: " + result
                state = 8
            Else If state = 9 Then
                readBack = value.value And value.value.length = 4 And ..
                    value.value[0] = 80 And value.value[1] = 73 And ..
                    value.value[2] = 78 And value.value[3] = 71
                If Not readBack Then RuntimeError "Readback mismatch"
                result = BLEDisconnect(connectionHandle)
                If result <> 0 Then RuntimeError "Disconnect failed: " + result
                state = 10
            End If
        Case EVENT_BLEWRITECOMPLETE
            If state = 8 Then
                If EventData() <> 0 Then RuntimeError "Write status: " + EventData()
                wrote = True
                result = targetCharacteristic.Read()
                If result <> 0 Then RuntimeError "Readback failed: " + result
                state = 9
            End If
        Case EVENT_BLEDISCONNECTED
            If state = 10 Then
                Print "NRF52_BLE_GATT_CLIENT,service=pass,characteristic=pass," + ..
                    "descriptor=" + descriptorRead + ",read=" + readInitial + ..
                    ",write=" + wrote + ",readback=" + readBack + ",disconnect=pass"
                state = 11
            Else If state > 1 And state < 10 Then
                RuntimeError "Unexpected disconnect"
            End If
    End Select
    If state = 11 Then SleepMilliseconds(1000) Else SleepMilliseconds(1)
Wend
