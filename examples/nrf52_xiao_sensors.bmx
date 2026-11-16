SuperStrict

Framework BRL.StandardIO
Import Embedded.Sensors.Motion
Import Embedded.Sensors.Temperature

Delay 2000
Print "SENSORS,motion=" + MotionSensorCount() + ",temperature=" + TemperatureSensorCount()

While True
    Local ax:Float, ay:Float, az:Float
    Local gx:Float, gy:Float, gz:Float
    Local stamp:Long
    Local celsius:Float
    Local motionOK:Int = MotionRead(0, ax, ay, az, gx, gy, gz, stamp)
    Local temperatureOK:Int = TemperatureReadCelsius(0, celsius)
    If motionOK And temperatureOK Then
        Print "SENSORS,ok=1,accel=" + ax + "," + ay + "," + az + ..
            ",gyro=" + gx + "," + gy + "," + gz + ..
            ",tempC=" + celsius + ",stampUs=" + stamp
    Else
        Print "SENSORS,ok=0,motion=" + motionOK + ",temperature=" + temperatureOK
    End If
    Delay 3000
Wend
