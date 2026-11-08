' Build only: bmk makeapp -r -l nrf52 -g arm -board promicro_nrf52840 nrf52_thread_echo.bmx
' After flashing, copy the printed dataset into the peer's ThreadJoin call.
SuperStrict

Framework BRL.StandardIO
Import NRF52.Network.Thread
Import Embedded.Network.Thread
Import NRF52.System.Time

Local result:Int = ThreadStartSaved()
If result = ThreadErrorNoSavedNetwork Then
	result = ThreadForm("BMXThread", 15)
	If result <> 0 Then RuntimeError "ThreadForm failed: " + result
	Print "THREAD_NETWORK=formed"
Else If result <> 0 Then
	RuntimeError "ThreadStartSaved failed: " + result
Else
	Print "THREAD_NETWORK=resumed"
End If
Print "THREAD_DATASET=" + ThreadActiveDataset()
Local role:Int = ThreadWaitForAttached()
If role < ThreadRoleChild Then RuntimeError "Thread attach failed: " + role
If ThreadUDPOpen(12345) <> 0 Then RuntimeError "Thread UDP bind failed"

Local packet:Byte[512]
Local lastAddressCheck:UInt
Local lastAddresses:String
While True
	If MilliSecs() - lastAddressCheck >= 5000 Then
		Local addresses:String
		For Local index:Int = 0 To 7
			Local address:String = ThreadIPv6Address(index)
			If address = "" Then Exit
			addresses :+ address + "~n"
		Next
		If addresses <> lastAddresses Then
			Print "THREAD_IPV6_CHANGED"
			For Local index:Int = 0 To 7
				Local address:String = ThreadIPv6Address(index)
				If address = "" Then Exit
				Print "THREAD_IPV6=" + address
			Next
			lastAddresses = addresses
		End If
		lastAddressCheck = MilliSecs()
	End If
	Local source:String, port:Int
	Local length:Int = ThreadUDPReceive(packet, source, port)
	If length >= 0
		Print "THREAD_RX=" + length + " from " + source + ":" + port
		Local sent:Int = ThreadUDPSend(source, port, packet, length)
		Print "THREAD_ECHO=" + sent
	End If
	SleepMilliseconds(20)
Wend
