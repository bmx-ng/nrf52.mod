SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import NRF52.Network.BLE
Import NRF52.System.Time

If BLEGATTReset() <> 0 Then RuntimeError "GATT reset failed"
Local service:TBLEGATTService = CreateBLEGATTService("FFF0")
If Not service Then RuntimeError "Service creation failed: " + BLEGATTLastError()
Local protectedValue:TBLEGATTCharacteristic = service.AddCharacteristic("FFF1", ..
    BLEGATTRead | BLEGATTReadEncrypted, [Byte(83), Byte(69), Byte(67)], 16)
If Not protectedValue Then RuntimeError "Characteristic creation failed: " + BLEGATTLastError()

Local result:Int = BLEConfigureSecurity(BLEIOCapabilityNone, True, False, True)
If result <> 0 Then RuntimeError "Security configuration failed: " + result
result = BLEInitialize("NRF Security")
If result <> 0 Then RuntimeError "BLE init failed: " + result
Print "NRF52_BLE_SECURITY,bonds_at_start=" + BLEBondCount()
result = BLEStartAdvertising(service, 0, True, True)
If result <> 0 Then RuntimeError "Advertising failed: " + result
Print "NRF52_BLE_SECURITY,advertising=pass"

Local pollCount:Int
While True
    Select PollEvent()
        Case EVENT_BLECONNECTED
            Print "NRF52_BLE_SECURITY,connected=pass"
        Case EVENT_BLESECURITYCHANGED
            Local state:TBLESecurityEvent = TBLESecurityEvent(EventExtra())
            Print "NRF52_BLE_SECURITY,status=" + state.status + ..
                ",encrypted=" + state.encrypted + ",bonded=" + state.bonded + ..
                ",key_size=" + state.keySize + ",bond_count=" + BLEBondCount()
        Case EVENT_BLEDISCONNECTED
            Print "NRF52_BLE_SECURITY,disconnected=pass,bond_count=" + BLEBondCount()
    End Select
    pollCount :+ 1
    If pollCount Mod 30 = 0 Then Print "NRF52_BLE_SECURITY,bond_count=" + BLEBondCount()
    SleepMilliseconds(100)
Wend
