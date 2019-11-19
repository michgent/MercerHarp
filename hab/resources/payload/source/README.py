"""
from sensor:

each sensor in different thread.
put together dictionary of form

        self.format=   {
                    "Type":"SR",
                    "Sensor":self.name,
                    "Time":"",
                    "Temperature":"",
                    "Pressure":""
                        }
turn into json string
make tuple of (self.name, sensorjsonstring)
put into qeueu
read from qeueu in main thread, get list, emtpy if nothing


in mainthread, have sensordict and gpsdict

        self.sensedict={"sensorname":"straight sensor reading json str", ... }

self.gpsdict={
            "Alt":"",
            "Lat":"",
            "Long":"",
            "Sat":"",
            "GPSTime":"",
            "Quality":"NA"           
            }

in main thread, use SensorRead()
if lame sensor, straight change sensor dict ; self.sensordict[readingtuple[0]]=readingtuple[1]
if gps sensor, use method to update gpsdict ;updategpsdict(jsongps)

pass that tuple to filereader self.PassSensorreadingtofilehandler(readingtuple)



filehandler stuff
---------------------------------------
each sensor has its own file and buffer list
startup:
make new folder based on gps date
take list of sensors and  for each one make empty list and "sensorname.txt"
put that in dictlistsensors
    self.dictlistsensors={} #{"sensorname":[listoflastsensorreadingsjsons,filenamestring]} or  #{"sensorname":[ [],filenamestring]}   

when get new sensorread, run sensortupleadd(self, latestsensortuple)
        sensorname=latestsensortuple[0]
        self.dictlistsensors[sensorname][0].append(latestsensortuple[1]+"\n")

        in the dict, access the list, add the jsonstring from the tuple with a end of line character
        cant use tuple instead of list because immutable, cant change list inside

when list of jsons is greater than 59, dump them into a the file for the sensor
empty the list
repeat





comms handling
--------------------------------------------

serial output pipeline

in main:
make json string message and pass to self.passmessagetoserial(msg)
that goesto comm handler and runs messagetowriteserial(messgstring)
reach into serialhandler and try to put msg string in queue to write out

in serial handler:
in main loop run messagewritemaybe(self), will get string from qeueu and do a serial write



serial input pipeline

in serial handler:
if firstreading is not stageone check or None,put into serial read queue: PutMessageInQueue(firstreading)

in comms handler:
run CheckSerialReadMessageQueue(self) to get list of json strings received

in Main:
run self.comms.CheckSerialReadMessageQueue() to get list of json strings




message handling
---------------------------
in main:
mainmessagereader(self) gets list of recived messages
if has messages, run self.determinemessagetype(messg)
loads the json string and check dict key "type"

if type==COMMAND, self.handleCOMMANDS(messagedict)

if type==REQUEST, get sensor name and self.handleREQUESTS(sensorname)

if type==RSGPS, self.sendshortgps()


"""
