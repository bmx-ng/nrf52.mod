SuperStrict

Framework BRL.StandardIO
Import Embedded.Random
Import NRF52.System.Time

Local values:UInt[16]
Local distinct:Int
For Local i:Int = 0 Until values.length
    values[i] = EmbeddedRandomUInt()
    If i And values[i] <> values[0] Then distinct = True
Next

Local bytes:Byte[31]
Local filled:Int = EmbeddedFillRandom(bytes, bytes.length)
Local nonzero:Int
For Local value:Byte = EachIn bytes
    If value Then nonzero = True
Next

Local passed:Int = distinct And filled And nonzero And Not EmbeddedFillRandom(Null, 1) And EmbeddedFillRandom(Null, 0)
While True
    If passed Then
        Print "NRF52_RANDOM,source=pass,fill=pass,validation=pass"
    Else
        Print "NRF52_RANDOM,failed"
    End If
    SleepMilliseconds(1000)
Wend
