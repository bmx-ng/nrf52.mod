' Verify 2048-byte Block2 download and Block1 upload against a Thread peer.
SuperStrict

Framework BRL.StandardIO
Import NRF52.Network.Thread
Import NRF52.System.Time
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCoAPBlock

Const ServerIPv6:String = "<paste-mesh-ipv6-address>"

If ServerIPv6[0] = Asc("<") Then RuntimeError "Set ServerIPv6 before flashing"
If ThreadStartSaved() <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"
If ThreadCoAPStart() <> 0 Then RuntimeError "CoAP start failed"
If ThreadCoAPBlockSend(ServerIPv6, "large", ThreadCoAPGet) <> 0 Then RuntimeError "Block GET send failed"
Local response:Byte[4096], code:Int
Local started:UInt = MilliSecs()
Local gotGet:Int
While MilliSecs() - started < 30000
	Local length:Int = ThreadCoAPBlockResponse(response, code)
	If length <> -11 Then
		If length <> 2048 Or code <> ThreadCoAPContent Then RuntimeError "Block GET failed: " + length
		For Local i:Int = 0 Until length
			If response[i] <> Byte(i & $ff) Then RuntimeError "Block GET mismatch at " + i
		Next
		Print "THREAD_COAP_BLOCK_GET=pass"
		gotGet = True
		Exit
	End If
	SleepMilliseconds(20)
Wend
If Not gotGet Then RuntimeError "Block GET timeout"

Local payload:Byte[2048]
For Local i:Int = 0 Until payload.length
	payload[i] = Byte(i & $ff)
Next
If ThreadCoAPBlockSend(ServerIPv6, "large", ThreadCoAPPost, payload) <> 0 Then RuntimeError "Block POST send failed"
started = MilliSecs()
While MilliSecs() - started < 30000
	Local length:Int = ThreadCoAPBlockResponse(response, code)
	If length <> -11 Then
		If length <> 2 Or code <> ThreadCoAPChanged Or response[0] <> Asc("O") Or response[1] <> Asc("K") Then RuntimeError "Block POST failed: " + length
		Print "THREAD_COAP_BLOCK_POST=pass"
		While True
			SleepMilliseconds(1000)
		Wend
	End If
	SleepMilliseconds(20)
Wend
RuntimeError "Block POST timeout"
