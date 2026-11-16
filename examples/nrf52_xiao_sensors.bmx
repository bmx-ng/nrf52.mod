SuperStrict

Framework BRL.StandardIO
Import Embedded.Sensors.Motion
Import Embedded.Sensors.Temperature

Delay 2000
Print "SENSORS,motion=" + MotionSensorCount() + ",temperature=" + TemperatureSensorCount()
Local motion:TMotionSensor = MotionSensorAt(0)
Local temperature:TTemperatureSensor = TemperatureSensorAt(0)

While True
    Local ax:Float, ay:Float, az:Float
    Local gx:Float, gy:Float, gz:Float
    Local stamp:Long
    Local celsius:Float
    Local motionOK:Int
    Local temperatureOK:Int
    If motion Then motionOK = motion.Read(ax, ay, az, gx, gy, gz, stamp)
    If temperature Then temperatureOK = temperature.ReadCelsius(celsius)
    If motionOK And temperatureOK Then
        Print "SENSORS,ok=1,accel=" + ax + "," + ay + "," + az + ..
            ",gyro=" + gx + "," + gy + "," + gz + ..
            ",tempC=" + celsius + ",stampUs=" + stamp
    Else
        Print "SENSORS,ok=0,motion=" + motionOK + ",temperature=" + temperatureOK
    End If
    Delay 3000
Wend
