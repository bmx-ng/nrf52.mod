' Repeatedly browse two named Thread services and verify their TXT and UDP data.
SuperStrict

Framework BRL.StandardIO
Import NRF52.Network.Thread
Import NRF52.System.Time
Import Embedded.Network.Thread
Import Embedded.Network.ThreadDiscovery

If ThreadStartSaved() <> 0 Then RuntimeError "No saved Thread network"
Print "THREAD_NETWORK=resumed"
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"
If ThreadServiceClientEnable() <> 0 Then RuntimeError "SRP client enable failed"
Local started:UInt = MilliSecs()
While Not ThreadServiceClientRunning() And MilliSecs() - started < 30000
	SleepMilliseconds(50)
Wend
If Not ThreadServiceClientRunning() Then RuntimeError "SRP server not found"
Print "THREAD_SRP_CLIENT=running"

Local socket:TThreadUDPSocket = TThreadUDPSocket.Open(12346)
If Not socket Then RuntimeError "UDP bind failed"
Local browser:TThreadServiceBrowser = TThreadServiceBrowser.Start("_bmxecho._udp", 5000)
If Not browser Then RuntimeError "Browse start failed"
Local payload:Byte[] = [Byte(Asc("P")), Byte(Asc("I")), Byte(Asc("N")), Byte(Asc("G"))]
Local reply:Byte[512]
While True
	Local state:Int = browser.Poll()
	If state = 2 Then
		Local labels:String[] = browser.Results()
		Print "THREAD_CATALOG_COUNT=" + labels.length
		For Local label:String = EachIn labels
			If ThreadServiceResolve(label, "_bmxecho._udp") <> 0 Then Continue
			Local address:String, port:Int
			started = MilliSecs()
			While ThreadServiceResolveState(address, port) = 1 And MilliSecs() - started < 10000
				SleepMilliseconds(50)
			Wend
			If ThreadServiceResolveState(address, port) <> 0 Then Continue
			Local txt:String[] = ThreadTXTDecode(ThreadServiceResolvedTXT())
			Print "THREAD_SERVICE=" + label + ":" + port
			If txt Then
				For Local entry:String = EachIn txt
					Print "THREAD_TXT=" + entry
				Next
			End If
			socket.Send(address, port, payload)
			started = MilliSecs()
			While MilliSecs() - started < 3000
				Local source:String, sourcePort:Int
				Local length:Int = socket.Receive(reply, source, sourcePort)
				If length = 4 And reply[0] = Asc("P") And reply[1] = Asc("I") And ..
					reply[2] = Asc("N") And reply[3] = Asc("G") Then
					Print "THREAD_CATALOG_ECHO=pass " + label
					Exit
				End If
				SleepMilliseconds(20)
			Wend
		Next
	Else If state < 0 Then
		Print "THREAD_BROWSE_ERROR=" + state
	End If
	SleepMilliseconds(50)
Wend
