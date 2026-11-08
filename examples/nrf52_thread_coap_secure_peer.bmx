' Send a CoAP GET over a Thread DTLS connection.
SuperStrict

Framework BRL.StandardIO
Import NRF52.Network.Thread
Import NRF52.System.Time
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCoAPSecure

Const ServerIPv6:String = "<paste-mesh-ipv6-address>"
Local psk:Byte[16]
If ServerIPv6[0] = Asc("<") Or psk[0] = 0 Then RuntimeError "Set peer address and private PSK before flashing"

If ThreadStartSaved() <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"
If ThreadCoAPSecureStart(psk, "example-peer") <> 0 Then RuntimeError "Secure CoAP start failed"
If ThreadCoAPSecureConnect(ServerIPv6) <> 0 Then RuntimeError "Secure CoAP connect failed"
Local started:UInt = MilliSecs()
While ThreadCoAPSecureConnection() = ThreadCoAPSecureConnecting And MilliSecs() - started < 30000
	SleepMilliseconds(20)
Wend
If ThreadCoAPSecureConnection() <> ThreadCoAPSecureConnected Then RuntimeError "DTLS handshake failed"
Print "THREAD_COAP_SECURE_CONNECTED=pass"
If ThreadCoAPSecureSend("secure", ThreadCoAPGet) <> 0 Then RuntimeError "Secure GET failed"
Local payload:Byte[1024], code:Int
started = MilliSecs()
While MilliSecs() - started < 30000
	Local length:Int = ThreadCoAPSecureResponse(payload, code)
	If length >= 0 Then
		If length <> 2 Or code <> ThreadCoAPContent Or payload[0] <> Asc("O") Or payload[1] <> Asc("K") Then RuntimeError "Secure response mismatch"
		Print "THREAD_COAP_SECURE_GET=pass"
		Exit
	Else If length <> -11 Then
		RuntimeError "Secure response failed: " + length
	End If
	SleepMilliseconds(20)
Wend
While True
	SleepMilliseconds(1000)
Wend
