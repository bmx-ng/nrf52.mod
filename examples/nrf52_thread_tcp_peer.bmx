' Connect to a TCP echo server over the saved Thread mesh.
SuperStrict

Framework BRL.StandardIO
Import NRF52.Network.Thread
Import NRF52.System.Time
Import Embedded.Network.Thread
Import Embedded.Network.ThreadTCP

Const EchoIPv6:String = "<paste-mesh-ipv6-address>"
If EchoIPv6[0] = Asc("<") Then RuntimeError "Set EchoIPv6 before flashing"
If ThreadStartSaved() <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"
If ThreadTCPOpen() <> 0 Or ThreadTCPConnect(EchoIPv6, 12360) <> 0 Then RuntimeError "TCP connect failed"
Local started:UInt = MilliSecs()
While ThreadTCPState() = ThreadTCPConnecting And MilliSecs() - started < 30000
	SleepMilliseconds(20)
Wend
If ThreadTCPState() <> ThreadTCPConnected Then RuntimeError "TCP handshake failed: " + ThreadTCPState()
Print "THREAD_TCP_CONNECTED=pass"
Local payload:Byte[] = [Byte(Asc("P")), Byte(Asc("I")), Byte(Asc("N")), Byte(Asc("G"))]
If ThreadTCPSend(payload) <> payload.length Then RuntimeError "TCP send failed"
Local reply:Byte[1024]
Local chunk:Byte[1024]
started = MilliSecs()
Local received:Int
While MilliSecs() - started < 15000
	Local length:Int = ThreadTCPReceive(chunk)
	If length > 0 Then
		If received + length > payload.length Then RuntimeError "TCP length mismatch"
		For Local i:Int = 0 Until length
			reply[received + i] = chunk[i]
		Next
		received :+ length
		If received = payload.length Then
			For Local i:Int = 0 Until received
				If reply[i] <> payload[i] Then RuntimeError "TCP payload mismatch"
			Next
			Print "THREAD_TCP_ECHO=pass"
			Exit
		End If
	Else If length = 0 Then
		RuntimeError "TCP stream ended early"
	End If
	SleepMilliseconds(20)
Wend
If received <> payload.length Then RuntimeError "TCP echo timeout"
ThreadTCPEndSend()
While True
	SleepMilliseconds(1000)
Wend
