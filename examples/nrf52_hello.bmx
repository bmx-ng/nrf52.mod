SuperStrict

Framework BRL.StandardIO
Import BRL.Blitz
Import NRF52.System.Time
Import NRF52.Runtime
Import NRF52.IO.StandardIO

Type TProbe
    Field value:Int
End Type

Local probe:TProbe = New TProbe
probe.value = 7
Local values:Int[] = [11, 13]
Local caught:Int
Try
    Throw "nRF52 exception probe"
Catch problem:String
    caught = problem = "nRF52 exception probe"
End Try
Local before:ULong = MonotonicMilliseconds()
SleepMilliseconds(25)
Local elapsed:ULong = MonotonicMilliseconds() - before
Local passed:Int = probe.value + values[0] + values[1] = 31 And caught And elapsed >= 25 And ManagedContextValid() And ArenaCapacity() = 65536 And StandardIOInit()

While True
    If passed Then
        Print "NRF52_MANAGED,objects=pass,arrays=pass,exceptions=pass,time=pass"
    Else
        Print "NRF52_MANAGED,failed"
    End If
    Print "Hello from BlitzMax on nRF52840"
    SleepMilliseconds(1000)
Wend
