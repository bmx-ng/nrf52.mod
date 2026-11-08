' Subscribe to a Thread CoAP Observe resource and verify ordered notifications.
SuperStrict

Framework BRL.StandardIO
Import NRF52.Network.Thread
Import NRF52.System.Time
Import Embedded.Network.Thread
Import Embedded.Network.ThreadCoAPObserve

Const ServerIPv6:String = "<paste-mesh-ipv6-address>"

If ServerIPv6[0] = Asc("<") Then RuntimeError "Set ServerIPv6 before flashing"
If ThreadStartSaved() <> 0 Or ThreadWaitForAttached() < ThreadRoleChild Then RuntimeError "Thread start failed"
If ThreadCoAPStart() <> 0 Then RuntimeError "CoAP start failed"
If ThreadCoAPObserveSubscribe(ServerIPv6, "counter") <> 0 Then RuntimeError "Observe subscribe failed"
Local payload:Byte[1024], code:Int, sequence:UInt
Local count:Int, previous:UInt, started:UInt = MilliSecs()
While count < 3 And MilliSecs() - started < 30000
	Local length:Int = ThreadCoAPObserveRead(payload, code, sequence)
	If length >= 0 Then
		If length <> 1 Or code <> ThreadCoAPContent Then RuntimeError "Observe value failed"
		If count > 0 And sequence <> previous + 1 Then RuntimeError "Observe sequence failed"
		Print "THREAD_OBSERVE_VALUE=" + (payload[0] & $ff) + " SEQ=" + sequence
		previous = sequence
		count :+ 1
	Else If length <> -11 Then
		RuntimeError "Observe read failed: " + length
	End If
	SleepMilliseconds(20)
Wend
If count <> 3 Then RuntimeError "Observe timeout"
If ThreadCoAPObserveCancel() <> 0 Then RuntimeError "Observe cancel failed"
Print "THREAD_OBSERVE=pass"
While True
	SleepMilliseconds(1000)
Wend
