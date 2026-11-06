SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "Service creation failed"
Local characteristic:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead | BLEGATTWrite | BLEGATTNotify | BLEGATTReadAuthenticated | ..
    BLEGATTWriteAuthenticated | BLEGATTSubscribeAuthenticated, ..
    [Byte(83), Byte(69), Byte(67)], 16)
If Not characteristic Then RuntimeError "Characteristic creation failed: " + BLEGATTLastError()

Local result:Int = BLEConfigureSecurity(BLEIOCapabilityDisplayYesNo, True, True, True, True)
If result <> 0 Then RuntimeError "Security configuration failed: " + result
result = BLEInitialize("NRF Protected")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEForgetAllBonds()
If result <> 0 Then RuntimeError "Bond deletion failed: " + result
result = BLEStartAdvertising(service, 0, True, False)
If result <> 0 Then RuntimeError "Advertising failed: " + result
Print "NRF52_BLE_PROTECTED,advertising=pass"

Local authenticated:Int
Local wrote:Int
Local subscribed:Int
Local notified:Int
While True
    Select PollEvent()
        Case EVENT_BLECONNECTED
            If EventData() <> 0 Then RuntimeError "Connection failed: " + EventData()
            Print "NRF52_BLE_PROTECTED,connected=pass"
        Case EVENT_BLEPASSKEYACTION
            Local action:TBLEPasskeyEvent = TBLEPasskeyEvent(EventExtra())
            If Not action Or action.action <> BLEPasskeyActionNumericComparison Then ..
                RuntimeError "Unexpected passkey action"
            result = BLEConfirmPasskey(action.connectionHandle, True)
            If result <> 0 Then RuntimeError "Comparison failed: " + result
        Case EVENT_BLESECURITYCHANGED
            Local state:TBLESecurityEvent = TBLESecurityEvent(EventExtra())
            If Not state Or state.status <> 0 Then RuntimeError "Security failed"
            If state.authenticated And state.encrypted Then authenticated = True
        Case EVENT_BLEGATTWRITE
            If Not authenticated Then RuntimeError "Unauthenticated write was accepted"
            Local changed:TBLEGATTWriteEvent = TBLEGATTWriteEvent(EventExtra())
            If Not changed Or changed.characteristic <> characteristic Then ..
                RuntimeError "Wrong write characteristic"
            Local value:Byte[] = characteristic.Value()
            If Not value Or value.length <> 3 Or value[0] <> 85 Or ..
                    value[1] <> 80 Or value[2] <> 33 Then RuntimeError "Wrong write value"
            wrote = True
            Print "NRF52_BLE_PROTECTED,authenticated_write=pass"
        Case EVENT_BLEGATTSUBSCRIBE
            If Not authenticated Or Not wrote Or EventData() = 0 Then ..
                RuntimeError "Subscription arrived before authenticated write"
            subscribed = True
            result = characteristic.SetValue([Byte(80), Byte(85), Byte(83), Byte(72)], True)
            If result <> 0 Then RuntimeError "Notification failed: " + result
            Print "NRF52_BLE_PROTECTED,authenticated_subscribe=pass"
        Case EVENT_BLEGATTUPDATECOMPLETE
            If Not subscribed Then RuntimeError "Unexpected update completion"
            notified = True
            Print "NRF52_BLE_PROTECTED,notify=pass"
        Case EVENT_BLEDISCONNECTED
            If Not authenticated Or Not wrote Or Not subscribed Or Not notified Then ..
                RuntimeError "Protected access check incomplete"
            Print "NRF52_BLE_PROTECTED,complete=pass"
    End Select
    SleepMilliseconds(1)
Wend
