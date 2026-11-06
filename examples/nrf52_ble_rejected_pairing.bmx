SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "Service creation failed"
Local protectedValue:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead | BLEGATTReadAuthenticated, [Byte(83), Byte(69), Byte(67)], 16)
If Not protectedValue Then RuntimeError "Characteristic creation failed"

Local result:Int = BLEConfigureSecurity(BLEIOCapabilityDisplayYesNo, True, True, True, True)
If result <> 0 Then RuntimeError "Security configuration failed: " + result
result = BLEInitialize("NRF Reject Test")
If result <> 0 Then RuntimeError "BLE init failed: " + result
result = BLEForgetAllBonds()
If result <> 0 Or BLEBondCount() <> 0 Then RuntimeError "Bond deletion failed"
result = BLEStartAdvertising(service, 0, True, False)
If result <> 0 Then RuntimeError "Advertising failed: " + result
Print "NRF52_BLE_REJECT,advertising=pass"

Local attempt:Int
Local rejected:Int
Local authenticated:Int
While True
    Select PollEvent()
        Case EVENT_BLECONNECTED
            If EventData() <> 0 Then RuntimeError "Connection failed: " + EventData()
            attempt :+ 1
            Print "NRF52_BLE_REJECT,connected=" + attempt
        Case EVENT_BLEPASSKEYACTION
            Local action:TBLEPasskeyEvent = TBLEPasskeyEvent(EventExtra())
            If Not action Or action.action <> BLEPasskeyActionNumericComparison Then ..
                RuntimeError "Unexpected passkey action"
            result = BLEConfirmPasskey(action.connectionHandle, attempt <> 1)
            If result <> 0 Then RuntimeError "Comparison response failed: " + result
            Print "NRF52_BLE_REJECT,comparison_response=" + attempt
        Case EVENT_BLESECURITYCHANGED
            Local state:TBLESecurityEvent = TBLESecurityEvent(EventExtra())
            If Not state Then RuntimeError "Missing security event"
            If attempt = 1 Then
                If state.status <> 0 Then rejected = True
                Print "NRF52_BLE_REJECT,rejection_status=" + state.status
            Else If attempt = 2 Then
                If state.status = 0 And state.authenticated And state.encrypted Then ..
                    authenticated = True
                Print "NRF52_BLE_REJECT,retry_status=" + state.status + ..
                    ",authenticated=" + state.authenticated
            End If
        Case EVENT_BLEDISCONNECTED
            If attempt = 1 Then
                If Not rejected Or BLEBondCount() <> 0 Then ..
                    RuntimeError "Rejected pairing changed bonds or lacked failure event"
                Print "NRF52_BLE_REJECT,rejected=pass,bonds=0"
                result = BLEStartAdvertising(service, 0, True, False)
                If result <> 0 Then RuntimeError "Advertising restart failed: " + result
            Else If attempt = 2 Then
                If Not authenticated Or BLEBondCount() <> 1 Then ..
                    RuntimeError "Pairing retry failed"
                Print "NRF52_BLE_REJECT,retry=pass,bonds=1"
                attempt = 3
            End If
    End Select
    SleepMilliseconds(1)
Wend
