import serial
import base64
import time
import math
import threading
import queue
import random
import datetime
import enum
import os
import sys
import hashlib
import SensorDataHandler
import json
import COMMS
import time

class SensorTypeEnum(enum.Enum): #not currently used
    TEMP=1
    GPS=2
    CPUTEMP=3

class MessageFields(enum.Enum):
    Type=1
    Sensor=2
    Content=3


class MessageType:
    def __init__(self):
        self.command=   {   
                        "Type":"COMMAND",
                        "Action":"",
                        "Data":""
                        }

        self.Request=    {   
                        "Type":"REQUEST",
                        "Sensor":"",
                         }

        self.RequestResponse={   
                            "Type":"REQUESTRESPONSE",
                            "Sensor":"",
                            "Content":""
                             }


        self.MainRead2={
                        "Type":"Main",
                        "Settings":{"currentclocktime":"", "currentdate":"", "cutdownarmed":"False", "GPSstate":{"GPSTime":0, "Lat":0, "Long":0, "Alt":0, "Sats":0}}, #I feel like this needed 
                        "GPS":{"Alt":"", "Lat":"", "Long":""},
                        "Sensorslist": {"TEMP":"mainjsonstring","CPU":"stringjson"}                 
                        }

        self.MainRead3={
                        "Type":"Main",
                        "Settings":{"currentclocktime":"", "currentdate":"", "cutdownarmed":"False", "GPSstate":{"GPSTime":0, "Lat":0, "Long":0, "Alt":0, "Sats":0}}, #I feel like this needed 
                        "GPS":{"Alt":"", "Lat":"", "Long":""},
                        "Sensorslist": [{"Sensor": "Name", "Data": "Jsonstring"},{}]                 
                        }

        self.MainRead={
                        "Type":"Main",
                        "Settings":{}, #I feel like this needed 
                        "GPS":{},
                        "Sensorslist": []                            
                        }
        self.RequestSerialGPS={
                        "Type":"RSGPS"
                        }

        self.SendSerialGPS={
                        "Type":"SGPS",
                        "Al":"",
                        "Lo":"",
                        "La":"",
                        "Ti":""
            }




    def getnewRequestResponse(self,sensorname,sensorreadingjson):
        diction=self.RequestResponse.copy()
        diction["Sensor"]=sensorname
        diction["Content"]=sensorreadingjson
        jsonstring=json.dumps(diction)
        return jsonstring

    def makemain(self,gpsdict,settingsdict,sensordict, slist):
        maindict=self.MainRead.copy()
        gps=gpsdict.copy()
        setting=settingsdict.copy()
        sensor=sensordict.copy()
        maindict["GPS"]=gps
        #maindict["Sensorslist"]=sensor
        tempsensedict={"Sensor":"", "Data":""}
        for sens in slist:
            if not sens =="GPS":
                listitem=tempsensedict.copy()
                listitem["Sensor"]=sens
                listitem["Data"]=sensor[sens]
                maindict["Sensorslist"].add(listitem)
        maindict["Settings"]=setting
        jsonstring=json.dumps(maindict)
        return jsonstring

    def makeserialgps(self, gpsdict):
        short=self.SendSerialGPS.copy()
        gps=gpsdict.copy()
        short["Al"]=gps["Alt"]
        short["Lo"]=gps["Long"]
        short["La"]=gps["Lat"]
        short["Ti"]=gps["GPSTime"]
        jsonstring=json.dumps(short)
        return jsonstring


class FlightController: # the main central class

    def __init__(self):
        #self.sensorlist=["GPS","MCPTEMP","BMP","CPUTEMP","MCPBMPCPU"] #temp is pressure and temp sensor, CPUTEMP will be added
        self.sensorlist=["GPS","MCPBMPCPU"] #temp is pressure and temp sensor, CPUTEMP will be added
        #self.subsensorlist=["MCPBMPCPU"]
        self.sensordict={} #dictionary of latest sensor data to pull from 
                            #{"sensorname":"straight sensor reading json str", ... }

        self.gpsdict={"GPSTime":"NA",
                      "Lat":"NA",
                      "Long":"NA",
                      "Alt":"NA",
                      "Sats":"NA",
                      "Quality":"NA"
                  }

        self.settings={"currentclocktime":"", "currentdate":"", "cutdownarmed":"False", "GPSstate":0}

        self.filehandle=FileHandler(self.sensorlist)
        self.sense=SensorDataHandler.SensorHandler()
        self.messages=MessageType()
        self.comms=COMMS.CommsHandler()
        self.timesnotconnected=0
        self.mainloopsleepperiod=0.1
        self.mainthread=None
        self.settime=False #bool
        self.date="20191117" #string yyyyMMdd
        self.mainloopsleepperiod=0.1
        
        self.Startup()
        self.comms.ModemSerialStart()


    def Startup(self):
        self.CreateDict()
        #self.mainthreadstart()
        
    def CreateDict(self):
        for sensor in self.sensorlist:
            self.sensordict[sensor]=None
       
    def mainthreadstart(self):
        self.mainthread=threading.Thread(target=self.MainLoop)
        self.mainthread.start()
    
    def SensorRead(self):
        """
        The main functin to call when reading the sensors.
        pulls all from the sensor queue and processes them 
        also send them to filehandler for saving
        returns a 0 if no change, 1 if sensor change, 2 if gps change
        """
        latestdatalist=self.sense.ReadQeueu() #getting list of ("sensorname", "jsonstring")
        if latestdatalist!=None and len(latestdatalist)>0:
            for readingtuple in latestdatalist:
                #print(readingtuple[1])
                if readingtuple[0]=="GPS":
                    self.updategpsdict(readingtuple[1])
                    re=2
                else:
                    self.sensordict[readingtuple[0]]=readingtuple[1]
                    re=1

                self.PassSensorreadingtofilehandler(readingtuple)
                return re
                
        else:
            return 0
       
    def updategpsdict(self,jsongps):
        """
        updates the gps dict when a new sr is read
        """
        glist=["Alt","GPSTime","Sats","Long","Lat","Quality"]
        try:
            gdict=json.loads(jsongps)
            for field in glist:
                self.gpsdict[field]=gdict[field]

            if self.settime:
                gtime=gdict["GPSTime"]
                self.updatesystemtime(gtime, self.date)
                #self.updatesystemtime("05:17:35",self.date)
                self.settime=False

        except:
            print("gep sensor error")
            return None



    def PassSensorreadingtofilehandler(self, srtuple):
        """
        method to pass sensor readings to filer handler
        """
        self.filehandle.sensortupleadd(srtuple)

        
        ###
        ### set time date methods
        ###

    def commandsettimebase(self, jsonstr):
        """
        jsonstr shoudld be of utc time---date;  HH:mm:ss---yyyyMMdd
        """
        strlist=jsonstr.split("---")
        if not strlist.count("---")==1:
            self.updatesystemtime(strlist[0], strlist[1])       

    def commandsettimeown(self):
        self.settime=True    
           
    def updatesystemtime(self, stringoftime, stringdate): #update time from gps or base station
        #time hh:mm:ss 17:41:32
        #date date month day 20140401 yyyyMMdd
        #gpsutc 20140401 17:41:32
        gpsutc=stringdate+' '+stringoftime
        os.system('sudo date -u --set="%s"' % gpsutc)

        ###
        ###end set time date methods
        ###


    def mainmessagereader(self):
        """
        check messageqeueu, gets lists and sorts received messages, handles replies
        """
        lis=self.comms.CheckSerialReadMessageQueue()

        if lis.len()>0:# if true, has messages, if not, nothing or possible error
            for messg in lis:
                self.determinemessagetype(messg)
        else:
            pass
    
    def determinemessagetype(self, messagejson):
        """
        message type checker and handler. does writing replies too
        """
        try:
            messagedict=json.loads(messagejson)
        except:
            return None

        if messagedict["Type"]=="COMMAND":
            self.handleCOMMANDS(messagedict)

        elif messagedict["Type"]=="REQUEST":

            sensorname=messagedict["Sensor"]
            self.handleREQUESTS(sensorname)

        elif messagedict["Type"]=="RSGPS":
            self.sendshortgps()

        else:
            pass#bad message

    def handleREQUESTS(self,sensorname):
        responsejson=self.messages.getnewRequestResponse(sensorname,self.sensordict[sensorname]) #should be passing a jsonstring and name string
        self.passmessagetorocket(responsejson)

    def handleCOMMANDS(self,messagedict):
        action=messagedict["Action"]

        if action=="SETTIME":
            self.commandsettimebase(messagedict["Data"])

        elif  action=="SETTIMEOWN":
            self.commandsettimeown()
            
        elif action=="ARMCUT":
            pass

        elif action=="CUTNOW":
            pass
        

    def testloop(self):
        while True:
            change=self.SensorRead()
            #print(self.gpsdict)
            if change==2:
                self.sendshortgps()

            time.sleep(0.15)
            #self.sendmainframe()

    def MainLoop(self):
        """
        periodically check sensor queue, update gps, sensor dicts, send/handle messgaes
        """
        while True:
            #check sensor, updtaes gps and sensor dicts, sends to file handler
            change=self.SensorRead()

            #check connection status  bool tuple (serialconn, rocketconn)
            connectionstatus=self.comms.checkconnectionmain()

            #check readmessgae qeueues and handle commands/requests if needed
            self.mainmessagereader()

            if self.comms.AnyConnectionStatus:
                #if have any connection, reset timer
                self.timesnotconnected=0
            else:
                #if no connection increment timer
                self.timesnotconnected+=1





            if connectionstatus[1] and (change==1 or change==2): 
                #if connected to modem, and sensor or gps update send main frame
                self.sendmainframe()

            elif connectionstatus[0] and change==2:
                #if no modem but serial connected and gps changed, send gps short
                self.sendshortgps()

            elif not connectionstatus[0] and not connectionstatus[1]:
                #if nothing connected ... idk
                pass

            time.sleep(self.mainloopsleepperiod)

    def passmessagetorocket(self, messgstring):

        pass

    def passmessagetoserial(self, messgstring):
        self.comms.messagetowriteserial(messgstring)

    def sendmainframe(self):
        """

        """
        msg=self.messages.makemain(self.gpsdict, self.settings, self.sensordict, self.sensorlist)
        print(msg)
        self.passmessagetorocket(messgstring)


    def sendshortgps(self):#gpsdict,settingsdict,sensordict):
        """
        send and create a short gps message to the serial modem
        """
        msg=self.messages.makeserialgps(self.gpsdict)
        print(msg)
        self.passmessagetoserial(msg)





class FileHandler:

    def __init__(self, slist):
        self.dictlistsensors={} #{"sensorname":[listoflastsensorreadingsjsons,filename],}
        self.filehandlerstartup(datetime.datetime.today().strftime('%Y-%m-%d'),slist)

    def sensortupleadd(self, latestsensortuple):
        """
        needs tuple of (str name, str json)
        takes sensors readings and sorts them
        """
        sensorname=latestsensortuple[0]
        ddd=self.dictlistsensors
        self.dictlistsensors[sensorname][0].append(latestsensortuple[1]+"\n") 


        if len(self.dictlistsensors[sensorname][0])>25:
            list1=self.dictlistsensors[sensorname][0]
            filename=self.dictlistsensors[sensorname][1]
            self.dictlistsensors[sensorname][0]=self.listdump(list1,filename)
            
        else:
            pass
              

    def listdump(self, list1, filename):
        with open(filename,"a+") as f: #IMPORTANT, json string dont have end of line, does writeline() put on a EOL?
            f.writelines(list1)
            f.close
        list1=[]
        return list1     
        self.dictlistsensors[sensorname]=sensorlist



    def filehandlerstartup(self, gpsdate,sensorlist):
        """
        main starup method for file handler, sets up folders, file names, and sorting
        """
        self.directorysetup(gpsdate)
        self.addsensorlisttodict(sensorlist)

    def directorysetup(self,gpsdate):

        try:
            os.mkdir(gpsdate)
        except:
            pass
        os.chdir(os.getcwd()+"//"+gpsdate)

    def addsensorlisttodict(self, sensorlist):
        """
        main startup method  for dict setup
        """
        for sensor in sensorlist:
            self.addsensortodict(sensor)

    def addsensortodict(self, sensorname):
        """
        startup helper method for dict setup
        """
        sensorfilename=sensorname+".txt" # say txt, will be full of json strings
        listz=[]
        sensorlist=[listz, sensorfilename]
        self.dictlistsensors[sensorname]=sensorlist


main=FlightController()
#time.sleep(1)
#main.sendshortgps()
#for x in range (13):
#    main.sendshortgps()
#    time.sleep(0.8)
    
while True:
    change=main.SensorRead()
    if change==2:
        print(change)
        main.sendshortgps()
    time.sleep(0.15)

#for x in range(100):
#    time.sleep(0.75)
#    main.SensorRead()


