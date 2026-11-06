SuperStrict

Framework BRL.StandardIO
Import NRF52.System.Time

Print "NRF52_USB_INPUT,ready"
Local answer:String = Input("Send PING over USB serial: ")
While True
    If answer = "PING" Then
        Print "NRF52_USB_INPUT,read=pass"
    Else
        Print "NRF52_USB_INPUT,read=fail,value=" + answer
    End If
    SleepMilliseconds(1000)
Wend
