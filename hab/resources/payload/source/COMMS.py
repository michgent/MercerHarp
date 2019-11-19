import hashlib
import zlib
import threading
import json
import enum
import serial
import queue
import time

class JSONTypeEnum(enum.Enum): # very used
    COMMAND = 1
    REQUEST = 2
    REQUESTRESPONSE = 7
    SR = 3
    SHORT = 4# short mgs for gps in serialmode
    OLTX = 5#openloop tx
    NORMTX = 6



class CommsHandler:
    def __init__(self):
        self.serial = None
        self.Threadserial = None
        self.serialconnected = False

        self.Rocket = None
        self.threadRocket = None
        self.RocketConnected = False

        self.AnyConnectionStatus = False
        self.openloopmoderunning = False
        
    def ModemSerialSetup(self):
        self.serial=SerialHandler(self) 
        
    def ModemSerialThreadStart(self):
        self.serial.MainLoopStart()

    def ModemSerialStart(self):
        self.ModemSerialSetup()
        print("kkkkk")
        self.ModemSerialThreadStart()

    def ModemSerialloopEnd(self):
        self.serial.MainReadEnd()

    def ModemSerialClose(self):
        self.ModemSerialloopEnd()
        if not (self.serial.IsLoopRunning):
            self.serial.SerialClosePort()
        else:
            pass
        
    def CheckSerialReadMessageQueue(self):
        """
        return a list of jsonstrings
        """
        list1=[]
        if not self.serial.queuemessagetoread.empty(): 
            while not self.serial.queuemessagetoread.empty():
                try:
                    list1.append(self.serial.queuemessagetoread.get())# dont block, immediately check and see
                except:
                    print("Check mesg queue error")
                    break
            return list1
        else:
            return list1

    def CheckRocketReadMessageQueue(self):
        pass


#    def messagetowriteserial(self,messagejson):
 #       self.connectionserialcheck()
#        if self.modemConnected:
#            pass
#        elif self.serialconnected:
#            try:
#                self.serial.queuemessagetowrite.put(messagejson)
#            except:
#               print("error something")
#        else:# no connection, probably should dump message
#            pass
#

    def messagetowriteserial(self,messagejson):
        try:
            #print("comms queue1")
            self.serial.queuemessagetowrite.put(messagejson)
            #print("comms queue2")
        except:
            print("error something")



    def messagetowriterocket(self, messagejson):
        pass

    def connectionserialcheck(self):
        """
        check the serial connection
        """
        sercheck=self.serial.connected
        if sercheck:
            conn=True

        else:
            conn=False

        self.serialconnected=conn
        return conn

    def rocketconnectioncheck(self):
        """
        check rocket connection
        """
        conn=False
        self.RocketConnected=False
        return conn
        

    def checkconnectionmain(self):
        """
        main connection status checker, returns tuple of bool (serialconnected, rocketm5connected)
        """
        first=self.connectionserialcheck()
        second=self.rocketconnectioncheck()
        
        connecttup=(first,second)
        self.AnyConnectionStatus=self.AnyConnectionCheck(connecttup)
        return connecttup


    def AnyConnectionCheck(self, tup):
        if tup[0] or tup[1]:
            return True
        else:
            return False

class SerialHandler:
    portname="COM5" #windows
    portname1="/dev/ttyUSB0" #linux
    
    def __init__(self, parent1):
        print("serial init")
        self.Serial=serial.Serial(port=self.portname1, baudrate=115200, timeout=0.3)
        self.connected=False
        self.serialloop=True
        self.stageone="pingPING"
        self.stagetwo="PINGping"
        self.stagethree="SOsure"
        self.stagefour="SUREso"
        self.serialthread=None
        self.timecheck=None
        #self.queuetimecheck=queue.Queue(5)
        self.queuemessagetoread=queue.Queue(10)
        self.queuemessagetowrite=queue.Queue(10)
        self.parent=parent1
        self.confirm="YY"

    def packetmaker(self,messagestring): 
        #make haskstring
        hashstring=hashlib.md5(messagestring.encode()).hexdigest()
        sendstring=messagestring+"///"+hashstring+'\n'
        return sendstring  #jsonstringpart///hashstring

    def packetchecker(self, messagebytestring):
        messagestring=messagebytestring.decode()
        messagestring = messagestring.rstrip('\n')
        if messagestring.count("///")>0:
            msglist=messagestring.split("///") #assuming "///" only occurs once
            hashtocheck=msglist[1]
            origanalmsg=msglist[0]
            originalmsghash=hashlib.md5(origanalmsg.encode()).hexdigest()
            if hashtocheck==originalmsghash:
                return origanalmsg
            else:
                
                print("hash problem")
                return None
        else:
            print("hash problem 1")
            print(messagestring)
            return None

    def serialwrite(self, message1string): # heavily changing
        #return bool
        datastring=self.packetmaker(message1string).encode()
        #self.Serial.write(datastring)
        
        try:
            self.Serial.write(datastring)    
            while self.Serial.out_waiting>0:
                pass
            return True
        except serial.SerialTimeoutException:
            print("cant establish connection")
            return False
        except:
            #print("error")
            return False

    def serialread(self):
        #return string
        try:
            data=self.Serial.readline()
            gg=data.decode('UTF-8')
            if gg=="":
                return None
        except:
            print("reading error")
            return None
        
        if (len(data) > 1):
            message=self.packetchecker(data)
        else:
            message = None
        
        return message

    def PingCheck(self,checkone):
        #checkone=self.serialread()
        if checkone==self.stageone:
            if self.serialwrite(self.stagetwo):
                return True
            else:
                return False
        else:
            return False

    def MainLoop(self):
        while True:
            if self.serialloop:#if want to read        
                
                #first check if anything to write from queue

                
                firstreading=self.serialread()
                if firstreading==self.stageone:# connection check

                    if self.PingCheck(firstreading):
                        self.connected=True

                    else: #connection check failed
                        self.connected=False

                elif firstreading==None: # reading error
                    #print("serial error")
                    self.connected=False

                else: #possible message, put in queue
                    print("serial msg")
                    self.connected=True
                    self.PutMessageInQueue(firstreading)
                    #self.messagesendconfirm()
                    #print("message?")
                    
                self.messagewritemaybe()       

            else:#break loop
                break
            time.sleep(0.01)

    def MainReadEnd(self): # should kill the thread
        self.serialloop=False
        self.serialthread.join(1.0)
        if self.serialthread.is_alive():
            try:
                self.serialthread._stop()
            except: #if cant kill thread.....idk
                pass

    def MainLoopStart(self):
        self.serialthread=threading.Thread(target=self.MainLoop)
        self.serialthread.start()

    def IsLoopRunning(self):
        return self.serialthread.isAlive()
    
    def SerialClosePort(self):
        self.Serial.close()

    def messagesendconfirm(self):# call when successful receive message
        self.serialwrite(self.confirm)
        

    def messagewantconfirm(self):# call when send message
        if self.serialread()==self.confirm:
            return True
        else:
            return False

    def messagewritemaybe(self): #try to send message
        if not self.queuemessagetowrite.empty():
            #print("serial messageget")
            messagejson=self.queuemessagetowrite.get()
            #messagejson=json.dumps(messagedict)
            self.serialwrite(messagejson)
            #if self.messagewantconfirm():
                #pass
            #else:
                #pass
        else:
            pass

    def PutMessageInQueue(self,Msgjson):
        """
        takes a  json string and passes it into the read queue
        """
        try:
            #msgdict=json.loads(Msgjson)
            self.queuemessagetoread.put(Msgjson, True) #block if qeueu full, maybe change
        except:
            pass


