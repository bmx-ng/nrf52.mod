' Discover the S31 CoAP resource and exercise confirmable GET and POST.
SuperStrict

Framework BRL.StandardIO
Import NRF52.Network.Thread
Import NRF52.System.Time
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCoAP
Import Embedded.Network.ThreadDiscovery

If ThreadStartSaved() <> 0 Then RuntimeError "No saved Thread mesh"
Print "THREAD_NETWORK=resumed"
If ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread attach failed"
If ThreadServiceClientEnable() <> 0 Then RuntimeError "SRP client failed"
Local started:UInt = MilliSecs()
While Not ThreadServiceClientRunning() And MilliSecs() - started < 30000
	SleepMilliseconds(50)
Wend
If Not ThreadServiceClientRunning() Then RuntimeError "SRP server not found"
If ThreadCoAPStart() <> 0 Then RuntimeError "CoAP start failed"
If ThreadServiceResolve("bmx-coap", "_bmxcoap._udp") <> 0 Then RuntimeError "DNS resolve failed"
Local address:String, port:Int
started = MilliSecs()
While ThreadServiceResolveState(address, port) = 1 And MilliSecs() - started < 10000
	SleepMilliseconds(50)
Wend
If ThreadServiceResolveState(address, port) <> 0 Then RuntimeError "DNS service not found"
Print "THREAD_COAP_SERVICE=" + address + ":" + port

Local reply:Byte[512], code:Int
If ThreadCoAPSend(address, "status", ThreadCoAPGet, Null, port) <> 0 Then RuntimeError "GET send failed"
started = MilliSecs()
While ThreadCoAPResponse(reply, code) = -11 And MilliSecs() - started < 15000
	SleepMilliseconds(50)
Wend
If code <> ThreadCoAPContent Or reply[0] <> Asc("O") Or reply[1] <> Asc("K") Then RuntimeError "GET reply failed"
Print "THREAD_COAP_GET=pass"

Local payload:Byte[] = [Byte(Asc("P")), Byte(Asc("I")), Byte(Asc("N")), Byte(Asc("G"))]
If ThreadCoAPSend(address, "status", ThreadCoAPPost, payload, port) <> 0 Then RuntimeError "POST send failed"
started = MilliSecs()
While ThreadCoAPResponse(reply, code) = -11 And MilliSecs() - started < 15000
	SleepMilliseconds(50)
Wend
If code <> ThreadCoAPChanged Or reply[0] <> Asc("P") Or reply[1] <> Asc("I") Or ..
	reply[2] <> Asc("N") Or reply[3] <> Asc("G") Then RuntimeError "POST reply failed"
Print "THREAD_COAP_POST=pass"
While True
	SleepMilliseconds(1000)
Wend
