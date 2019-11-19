import subprocess
import shlex
import serial


def process(line):
	line=str(line)


def run_command(command,mylist, serial):
	#if
	process = subprocess.Popen(command, stdout=subprocess.PIPE, shell=True)
	while True:
		output = process.stdout.readline()
		output= str(output)
		if output == '' and process.poll() is not None:
			break
		if output:
			print(output)
			line=output
			x=output.startswith("APRS: KN4PXE-11>APRS",0,40)
			if x == True:
				time=line[30:37]
				long=line[37:45]
				lat=line[46:55]# lat and long are switched
				alt=line[64:72]
				final=time+","+long+","+lat+","+alt
				ser.write(str(final).encode())
				print(final)

			#process(output)
	rc = process.poll()
	return rc

mylist = []
#filename="test.txt"
ser = serial.Serial("/dev/ttyACM0")
run_command("rtl_fm -f 144.390M -s 22050 -g 100 - | multimon-ng -a AFSK1200 -A -t raw -", mylist,ser)
#run_command("python Documents/testloop.py",mylist)
#file=open(filename,"a")
#file.write(mylist[0])
#for line in mylist:
#	file.write(line+"\n")
#file.close()
