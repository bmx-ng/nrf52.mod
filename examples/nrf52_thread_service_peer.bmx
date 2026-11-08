' A commissioned nRF52840 peer discovers the echo node without a fixed IPv6 address.
SuperStrict

Framework BRL.StandardIO
Import NRF52.Network.Thread
Import NRF52.System.Time
Import Embedded.Network.Thread

Print "THREAD_BOOT=starting"
If ThreadStartSaved() <> 0 Then RuntimeError "No saved Thread network"
Print "THREAD_NETWORK=resumed"
Local role:Int = ThreadWaitForAttached()
If role < ThreadRoleChild Then RuntimeError "Thread attach failed: " + role
Print "THREAD_ROLE=" + role
If ThreadServiceClientEnable() <> 0 Then RuntimeError "SRP client enable failed"
Print "THREAD_SRP_CLIENT=waiting"
Local started:UInt = MilliSecs()
While Not ThreadServiceClientRunning() And MilliSecs() - started < 30000
	SleepMilliseconds(50)
Wend
If Not ThreadServiceClientRunning() Then RuntimeError "SRP server not found"
Print "THREAD_SRP_CLIENT=running"

Local socket:TThreadUDPSocket = TThreadUDPSocket.Open(12346)
If Not socket Then RuntimeError "Thread UDP bind failed"
Local payload:Byte[] = [Byte(Asc("P")), Byte(Asc("I")), Byte(Asc("N")), Byte(Asc("G"))]
Local reply:Byte[512]
While True
	Local result:Int = ThreadServiceBrowse("_bmxecho._udp")
	If result <> 0 Then Print "THREAD_BROWSE_START=" + result
	If result = 0 Then
		started = MilliSecs()
		While ThreadServiceBrowseState() = 1 And MilliSecs() - started < 10000
			SleepMilliseconds(50)
		Wend
		If ThreadServiceBrowseState() = 0 Then
			Local instanceName:String = ThreadServiceBrowseResult(0)
			If instanceName = "" Then Print "THREAD_SERVICE=none"
			If instanceName <> "" Then
				Print "THREAD_SERVICE=" + instanceName
				result = ThreadServiceResolve(instanceName, "_bmxecho._udp")
				If result = 0 Then
					Local address:String, port:Int
					started = MilliSecs()
					While ThreadServiceResolveState(address, port) = 1 And MilliSecs() - started < 10000
						SleepMilliseconds(50)
					Wend
					If ThreadServiceResolveState(address, port) = 0 Then
						Print "THREAD_ENDPOINT=" + address + ":" + port
						socket.Send(address, port, payload)
						started = MilliSecs()
						While MilliSecs() - started < 3000
							Local source:String, sourcePort:Int
							Local length:Int = socket.Receive(reply, source, sourcePort)
							If length = 4 And reply[0] = Asc("P") And reply[1] = Asc("I") And ..
								reply[2] = Asc("N") And reply[3] = Asc("G") Then
								Print "THREAD_SERVICE_ECHO=pass"
								Exit
							End If
							SleepMilliseconds(20)
						Wend
					End If
					If ThreadServiceResolveState(address, port) < 0 Then Print "THREAD_RESOLVE_STATE=" + ThreadServiceResolveState(address, port)
				End If
			End If
		Else
			Print "THREAD_BROWSE_STATE=" + ThreadServiceBrowseState()
		End If
	End If
	SleepMilliseconds(4000)
Wend
