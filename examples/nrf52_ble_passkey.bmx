SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "Service creation failed: " + BLEGATTLastError()
Local protectedValue:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead | BLEGATTReadAuthenticated, [Byte(83), Byte(69), Byte(67)], 16)
If Not protectedValue Then RuntimeError "Characteristic creation failed: " + BLEGATTLastError()

Local result:Int = BLEConfigureSecurity(BLEIOCapabilityKeyboardOnly, True, True, True, True)
If result <> 0 Then RuntimeError "Security configuration failed: " + result
result = BLEInitialize("NRF Passkey")
If result <> 0 Then RuntimeError "BLE init failed: " + result
If BLEBondCount() > 0
    Local bond:TBLEBond = BLEBondAt(0)
    If Not bond Or bond.status <> 0 Or Not bond.address Or ..
            bond.address.length <> 6 Then RuntimeError "Bond enumeration failed"
    Print "NRF52_BLE_PASSKEY,bond_enumeration=pass"
End If
result = BLEForgetAllBonds()
If result <> 0 Or BLEBondCount() <> 0 Then RuntimeError "Bond deletion failed: " + result
Print "NRF52_BLE_PASSKEY,bond_deletion=pass"
result = BLEStartAdvertising(service)
If result <> 0 Then RuntimeError "Advertising failed: " + result
Print "NRF52_BLE_PASSKEY,advertising=pass"

While True
    Select PollEvent()
        Case EVENT_BLECONNECTED
            Print "NRF52_BLE_PASSKEY,connected=pass"
        Case EVENT_BLEPASSKEYACTION
            Local action:TBLEPasskeyEvent = TBLEPasskeyEvent(EventExtra())
            If Not action Or action.action <> BLEPasskeyActionInput Then ..
                RuntimeError "Unexpected passkey action"
            ' Fixed test passkey shared with the ESP32 peer; use a fresh value in real apps.
            result = BLEProvidePasskey(action.connectionHandle, action.action, 123456)
            If result <> 0 Then RuntimeError "Passkey input failed: " + result
            Print "NRF52_BLE_PASSKEY,input=pass"
        Case EVENT_BLESECURITYCHANGED
            Local state:TBLESecurityEvent = TBLESecurityEvent(EventExtra())
            If state.status <> 0 Then RuntimeError "Pairing failed: " + state.status
            Print "NRF52_BLE_PASSKEY,authenticated=" + state.authenticated + ..
                ",bonded=" + state.bonded + ",key_size=" + state.keySize
        Case EVENT_BLEDISCONNECTED
            Print "NRF52_BLE_PASSKEY,disconnected=pass,bond_count=" + BLEBondCount()
    End Select
    SleepMilliseconds(1)
Wend
