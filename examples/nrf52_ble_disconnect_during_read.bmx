SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

SleepMilliseconds(3000)
Print "BLE_DROP,boot=pass"
Local result:Int = BLEInitialize("NRF Drop Client")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEStartScan(0, True, True)
If result <> 0 Then RuntimeError "Scan failed: " + result
Print "BLE_DROP,scanning=pass"

Local phase:Int = 1, state:Int = 1
Local connectionHandle:Int, mtu:Int
Local service:TBLEClientService
Local characteristic:TBLEClientCharacteristic
Local firstStatus:Int, firstCompleted:Int, firstDisconnected:Int
Local secondRead:Int
' Zephyr's newlib errno.h defines ENOTCONN as 128 on this target.
Const NRF_ENOTCONN:Int = -128
Local deadline:UInt = MilliSecs() + 30000
While True
    Select PollEvent()
        Case EVENT_BLESCANRESULT
            If state = 1 Then
                Local advertisement:TBLEAdvertisement = TBLEAdvertisement(EventExtra())
                If advertisement And advertisement.LocalName() = "ESP Drop Target" Then
                    Print "BLE_DROP,peer_found=pass"
                    result = BLEStopScan()
                    If result <> 0 Then RuntimeError "Stop scan failed: " + result
                    result = BLEConnectAdvertisement(advertisement)
                    If result <> 0 Then RuntimeError "Connect failed: " + result
                    state = 2
                End If
            End If
        Case EVENT_BLECONNECTED
            If state = 2 Then
                If EventData() <> 0 Then RuntimeError "Connect event failed: " + EventData()
                connectionHandle = EventX()
                Print "BLE_DROP,connected=" + phase
                result = BLEExchangeMTU(connectionHandle)
                If result <> 0 Then RuntimeError "MTU exchange failed: " + result
                state = 3
            End If
        Case EVENT_BLEMTUCHANGED
            If state = 3 Then
                mtu = BLEConnectionMTU(connectionHandle)
                If mtu < 247 Then RuntimeError "MTU too small: " + mtu
                Print "BLE_DROP,mtu=" + mtu
                result = BLEDiscoverServices(connectionHandle)
                If result <> 0 Then RuntimeError "Service discovery failed: " + result
                state = 4
            End If
        Case EVENT_BLESERVICEDISCOVERED
            If state = 4 Then
                Local found:TBLEClientService = TBLEClientService(EventExtra())
                If found And (found.UUID() = "fff0" Or found.UUID() = "FFF0") Then service = found
            End If
        Case EVENT_BLESERVICEDISCOVERYCOMPLETE
            If state = 4 Then
                If EventData() <> 0 Or Not service Then RuntimeError "Service missing"
                result = service.DiscoverCharacteristics()
                If result <> 0 Then RuntimeError "Characteristic discovery failed: " + result
                state = 5
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERED
            If state = 5 Then
                Local found:TBLEClientCharacteristic = TBLEClientCharacteristic(EventExtra())
                If found And (found.UUID() = "fff1" Or found.UUID() = "FFF1") Then characteristic = found
            End If
        Case EVENT_BLECHARACTERISTICDISCOVERYCOMPLETE
            If state = 5 Then
                If EventData() <> 0 Or Not characteristic Or Not characteristic.CanRead() Then RuntimeError "Characteristic missing"
                result = characteristic.Read()
                If result <> 0 Then RuntimeError "Read start failed: " + result
                Print "BLE_DROP,read_started=" + phase
                state = 6
            End If
        Case EVENT_BLEREADCOMPLETE
            If state = 6 Then
                If phase = 1 Then
                    firstStatus = EventData()
                    firstCompleted = True
                    Print "BLE_DROP,first_read_status=" + firstStatus
                Else
                    If EventData() <> 0 Then RuntimeError "Second read failed: " + EventData()
                    Local value:TBLEClientValueEvent = TBLEClientValueEvent(EventExtra())
                    If Not value Or Not value.value Or value.value.length <> 300 Then RuntimeError "Second read length mismatch"
                    For Local index:Int = 0 Until 300
                        If value.value[index] <> Byte(index & 255) Then RuntimeError "Second read value mismatch"
                    Next
                    secondRead = True
                    result = BLEDisconnect(connectionHandle)
                    If result <> 0 Then RuntimeError "Second disconnect failed: " + result
                    state = 7
                End If
            End If
        Case EVENT_BLEDISCONNECTED
            If phase = 1 And state = 6 Then
                firstDisconnected = True
                Print "BLE_DROP,first_disconnected=pass"
            Else If phase = 2 And state = 7 Then
                If firstStatus <> NRF_ENOTCONN Then RuntimeError "First read did not complete with ENOTCONN: " + firstStatus
                If Not secondRead Then RuntimeError "Second read missing"
                Print "NRF52_BLE_DROP_DURING_READ,mtu=" + mtu + ..
                    ",interrupted_read=pass,reconnect=pass,read_300=pass,disconnect=pass"
                state = 8
            End If
    End Select
    If phase = 1 And state = 6 And firstCompleted And firstDisconnected Then
        phase = 2
        service = Null
        characteristic = Null
        result = BLEStartScan(0, True, True)
        If result <> 0 Then RuntimeError "Scan after drop failed: " + result
        state = 1
    End If
    If state <> 8 And MilliSecs() > deadline Then RuntimeError "BLE drop test timed out at state " + state
    If state = 8 Then SleepMilliseconds(1000) Else SleepMilliseconds(1)
Wend
