SuperStrict

Framework BRL.EventQueue
Import BRL.StandardIO
Import Embedded.Hardware.GPIOEvents
Import Embedded.Runtime.Events
Import NRF52.System.Time

Const LEDPin:UInt = 15

Global received:Int
Global eventFlags:UInt
Global eventPin:Int = -1
Global eventTime:ULong

Function CaptureGPIOEvent:Object(hookId:Int, data:Object, context:Object)
    Local event:TEvent = TEvent(data)
    If event And event.id = EVENT_GPIOIRQ Then
        received :+ 1
        eventFlags :| UInt(event.data)
        eventPin = event.mods
        eventTime = GPIOIRQTimeUS(event)
    End If
    Return data
End Function

AddHook EmitEventHook, CaptureGPIOEvent

Local ready:Int = GPIOInit(LEDPin) And GPIOSetOutput(LEDPin) And GPIOPut(LEDPin, 0)
Local source:TGPIOIRQSource
If ready Then source = TGPIOIRQSource.Create(LEDPin, GPIOIRQEdgeRise | GPIOIRQEdgeFall)
ready = ready And source <> Null

If ready Then
    SleepMilliseconds(50)
    ready = GPIOPut(LEDPin, 1)
    SleepMilliseconds(50)
    ready = ready And GPIOPut(LEDPin, 0)
    SleepMilliseconds(50)
    PollSystem()
End If

Local pending:UInt = GPIOPendingIRQEvents(LEDPin)
Local taken:UInt = GPIOTakeIRQEvents(LEDPin)
Local passed:Int = ready And received >= 2 And ..
    (eventFlags & (GPIOIRQEdgeRise | GPIOIRQEdgeFall)) = ..
    (GPIOIRQEdgeRise | GPIOIRQEdgeFall) And ..
    (pending & taken) = pending And eventPin = Int(LEDPin) And ..
    eventTime > 0 And EmbeddedDeferredEventDropped() = 0

If source Then source.Close()
While True
    If passed Then
        Print "NRF52_GPIO_IRQ,rise=pass,fall=pass,managed=pass"
    Else
        Print "NRF52_GPIO_IRQ,failed,received=" + received + ",flags=" + eventFlags + ",pending=" + pending
    End If
    SleepMilliseconds(1000)
Wend
