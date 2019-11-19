import random
import queue
import threading
import datetime
import time
import json
import board
import busio
import adafruit_mcp9808
import serial
import pynmea2
import os
import adafruit_bmp3xx

class SensorHandler:

    def __init__(self):
        self.queuetimeout=.01
        self.mainqeue=queue.Queue()# should probably have a size limit
        self.gps=GPS(self)
        self.temp=MCPBMPCPU(self)
        self.ReadData=True


    def timestring(self):
        return datetime.datetime.now().strftime("%H:%M:%S")

    def ReadQeueu(self):
        quicklist=[]
        if not self.mainqeue.empty():
            while not self.mainqeue.empty() and self.ReadData: #must want to read data and queue must have stuff
                quicklist.append(self.mainqeue.get())
        return quicklist
                

class ParentSensor:
    def __init__(self,_parent):
        self.Thread=None
        self.parent=_parent
        self.name=None
        self.format=None

    def loop(self):
        pass

    def threadstart(self):
        self.Thread=threading.Thread(target=self.loop)
        self.Thread.start()


    def passonreading(self):
        readingstr=self.Getnew()
        nameandreading=self.maketuple(readingstr)
        self.parent.mainqeue.put(nameandreading)


    def maketuple(self,jsonstring):
        tup=(self.name,jsonstring)
        return tup    

class GPS:
    def __init__(self,_parent):
        self.Thread=None
        self.parent=_parent
        self.serialPort= serial.Serial("/dev/ttyS0",9600,timeout=0.5)
        self.name="GPS"
        self.format={ "Type":"SR",
                      "Sensor":self.name,
                      "GPSTime":"",
                      "Lat":"",
                      "Long":"",
                      "Alt":"",
                      "Sats":"",
                      "Quality":""
                  }
        self.threadstart()
        
    def threadstart(self):
        self.Thread=threading.Thread(target=self.loop)
        self.Thread.start()
        
    def paersegps(self, gpsstring):
        gpsstring=str(gpsstring.decode())
        if gpsstring.count('GGA')>0:
            msg= pynmea2.parse(str(gpsstring))
            return msg
        #elif gpsstring.count('ZDA')>0:
        #    msg= pynmea2.parse(str(gpsstring))
        #    return msg
            #print("Timestamp: %s -- Lat: %s %s -- Lon: %s %s -- Altitude: %s %s" % (msg.timestamp,msg.lat,msg.lat_dir,msg.lon,msg.lon_dir,msg.altitude,msg.altitude_units))    
    
    def loop(self):
        while True:
            print("1hhhh")
            str1=self.serialPort.readline()
            messg=self.paersegps(str1)
            
            if messg!=None:
                jstring=self.jsonoutputter(messg)
                self.passonreading(jstring)

    def jsonoutputter(self, msg):
        tempdict=self.format.copy()
        if msg.timestamp!="":
            tempdict["GPSTime"]=self.gettime(msg)
        tempdict["Lat"]=self.getlat(msg)
        tempdict["Long"]=self.getlong(msg)
        tempdict["Alt"]=self.getalt(msg)
        tempdict["Sats"]=self.getsat(msg)
        tempdict["Quality"]=self.getfixquality(msg)
        #print(tempdict)
        jsonstr=json.dumps(tempdict)
        #print(jsonstr)
        return jsonstr

    def getalt(self,msg):
        if msg.altitude!="" and msg.altitude!=None:
            alt=msg.altitude
            return str(alt)
        else:
            return "NA"
    def getfixquality(self, msg):
        fix=msg.gps_qual
        return str(fix)

    def gettime(self,msg):
        if msg.timestamp!="" and msg.timestamp!=None:
            timee=msg.timestamp
            return str(timee)
        else:
            return "NA"

    def getlat(self,msg):
        if msg.lat!="" and msg.lat_dir!="":
            lat=float(msg.lat)
            if msg.lat_dir=="S":
                lat=-1*lat
            else:
                lat=lat
            lat=self.convertcorrdinates(str(lat))
        #convert to string
            return str(lat)
        else:
            return "NA"

    def getlong(self,msg):
        if msg.lon!="" and msg.lon_dir!="":
            long=float(msg.lon)
            if msg.lon_dir=="W":
                long=-1*long
            else:
                long=long
            long=self.convertcorrdinates(str(long))
            return str(long)
        else:
            return "NA"

    def getsat(self,msg):
        if msg.num_sats!="":
            sat=msg.num_sats
            return str(sat)
        else:
            return "NA"

    def passonreading(self, readingstr):
        nameandreading=self.maketuple(readingstr)
        
        self.parent.mainqeue.put(nameandreading)

    def maketuple(self,jsonstring):
        tup=(self.name,jsonstring)
        return tup       

    def convertcorrdinates(self, numberstring):
        dd=int(float(numberstring)/100)
        ss=(float(numberstring) - dd * 100)/60
        final=dd+ss
        return str(final)
    
class MCPBMPCPU:
    def __init__(self,_parent):
        self.Thread=None
        self.parent=_parent
        self.name="MCPBMPCPU"
        self.format={ "Type":"SR",
                      "Sensor":self.name,
                      "Time":"",
                      "Temp-precise":"",
                      "Temperature-rough":"",
                      "Pressure":"",
                      "Temp-Cpu":""
                    }
        self.i2c_bus = busio.I2C(board.SCL, board.SDA)
        self.bmp = adafruit_bmp3xx.BMP3XX_I2C(self.i2c_bus)
        self.mcp = adafruit_mcp9808.MCP9808(self.i2c_bus)
        self.bmp.pressure_oversampling = 8
        self.bmp.temperature_oversampling = 2
        self.readfreq=1
        self.threadstart()


    def threadstart(self):
        self.Thread=threading.Thread(target=self.loop)
        self.Thread.start()
        
    def loop(self):
        while True:
            timestrs=datetime.datetime.now().strftime("%H:%M:%S")
            tempbm=self.gettempbmp()
            pres=self.getpressurebmp()
            tempmc=self.gettempmcp()
            tempcpu=self.gettempcpu()
            jstring=self.jsonoutputter(tempmc,tempbm,pres,tempcpu,timestrs)
            #print()
            self.passonreading(jstring)
            time.sleep(self.readfreq)
          
    def gettempmcp(self):
        tempC = self.bmp.temperature
        tempF = tempC * 9 / 5 + 32
        return tempF

    def gettempbmp(self):
        tempC = self.mcp.temperature
        tempF = tempC * 9 / 5 + 32
        return tempF
    
    def getpressurebmp(self):
        pres = self.bmp.pressure
        return pres

    def gettempcpu(self):
        temp = os.popen("vcgencmd measure_temp").readline()
        t=temp.replace("temp=","")
        t=t.rstrip("\n")
        t=t.rstrip("'C")
        num=float(t)
        num=(num* (9 / 5)) + 32
        return str(num)
            
    def jsonoutputter(self, tempmc, tmpbm, pres, cpu,timestr):
        tempdict=self.format.copy()
        tempdict["Time"]=timestr
        tempdict["Temp-precise"]=tempmc
        tempdict["Temperature-rough"]=tmpbm
        tempdict["Pressure"]=pres
        tempdict["Temp-Cpu"]=cpu
        jsonstr=json.dumps(tempdict)
        return jsonstr

    def passonreading(self, readingstr):
        nameandreading=self.maketuple(readingstr)
        self.parent.mainqeue.put(nameandreading)

    def maketuple(self,jsonstring):
        tup=(self.name,jsonstring)
        return tup

#j=SensorHandler()
#j.gps.loop()
