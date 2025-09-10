#!/usr/bin/env python3


from evdev import InputDevice, categorize, ecodes, KeyEvent, list_devices
import select
import time
import serial
import serial.tools.list_ports # for reading the available ports
import os
import datetime

#hex numbers for rfid reader found using lsusb
RFID_VENDOR_ID = 0x0801
RFID_PRODUCT_ID = 0x0001

#hex numbers for arduino found using lsusb
ARDUINO_VENDOR_ID = 0x2341
ARDUINO_PRODUCT_ID = 0x0043

#const values
SCRIPT_PATH = os.path.dirname(os.path.abspath(__file__))
LOG_FILENAME = os.path.join(SCRIPT_PATH, "keybox_log.txt")
ID_FILENAME = os.path.join(SCRIPT_PATH, "idlist.txt")
BAUD_RATE = 9600
TIMEOUT = 1

#used for reading the ID list txt file change file name if ever changing
def read_authorized_ids(filename):
	try:
		with open(filename, "r") as f:
			listArray = [line.strip().split('_')[0] for line in f if line.strip()]
	except FileNotFoundError:
		print(f"Read File not found {filename}")
		return None
	return listArray
	
#used to find the rfid reader and make arduino connection
def find_device(vendor_id, product_id):
	#trys arduino connection first
	try:
		arduino_port = None
		ports = list(serial.tools.list_ports.comports())
		for p in ports:
			#checks for the vender id and product
			if p.vid == vendor_id and p.pid == product_id:
				arduino_port = p.device
				break;
		#if arduino port is not None
		if arduino_port:
			print(f"Found Arduino on Port: {arduino_port}")
			ardConnection = serial.Serial(arduino_port, BAUD_RATE, timeout=TIMEOUT)
			ardConnection.reset_input_buffer()
			print("Arduino connected!")
			return ardConnection
	except serial.SerialException as s:
		print(f"Serial fail error {s}")
	except Exception as e:
		print(f"Unknown error as serial connection {e}")
		
	if arduino_port == None:
		devices = [InputDevice(path) for path in list_devices()] #looks at all devices plugged in
		for device in devices:
			if device.info.vendor == vendor_id and device.info.product == product_id:
				#if the arduino was not found this will return nothing
				if vendor_id == ARDUINO_VENDOR_ID and product_id == ARDUINO_PRODUCT_ID:
					return None
				return device
		print("device not found trying again")
		return None

#used for writing the file
def write_to_file(newTextArray, filename):
	try:
		with open(filename, "w") as f:
			for newString in newTextArray:
				f.write(f"{newString}\n")
	except IOError as e:
		print(f"There was an IO error: {e}")

#use to clear the log file and stop from getting to big
def clear_logs(filename):
	logList = read_authorized_ids(filename)
	if len(logList) > 20:
		newLogList = logList[len(logList)-1]
		write_to_file(newLogList, filename)
#main function running the program
def main():
	#gets ids from stored file exits program on error
	errNumber = 0
	while True:
		authorized_ids = read_authorized_ids(ID_FILENAME)
		if authorized_ids == None:
			print(f"Error:{errNumber} ID file is not working or opening retrying in 5 seconds")
			errNumber += 1
			time.sleep(5)
		else:
			errNumber = 0
			break
			
	#sets a blank update tag to use
	update_tag_id = ""
	
	#do while statment for getting arduino connection
	while True:
		ardConnection = find_device(ARDUINO_VENDOR_ID, ARDUINO_PRODUCT_ID)
		if ardConnection == None:
			print(f"Error:{errNumber} Arduino is not connected will try again in 5 seconds")
			errNumber += 1
		else:
			print("Arduino found continuing")
			time.sleep(3)
			ardConnection.write(b"PI_READY")
			errNumber = 0
			break
		#wait five seconds before trying again
		time.sleep(5)
		
	#do while statment for getting rfid connection
	while True:
		rfid_device = find_device(RFID_VENDOR_ID, RFID_PRODUCT_ID)
		if rfid_device == None:
			print(f"Error:{errNumber} RFID not connected trying agian in five seconds")
			errNumber += 1
		else:
			print("RFID found continuing")
			#incase arduino is not ready for input
			time.sleep(2)
			ardConnection.write(b"RFID_READY")
			errNumber = 0
			break
		#wait five seconds
		time.sleep(5)
		
	#try statement for getting card tag and comparing it
	try:
		#grabs the reader so that it cant print to notepad
		rfid_device.grab()
		print("Listening for ID swipe or Arduino Messages")
		current_tag_id = ""
		#boolean value for update mode
		update_mode = False
		update_tag_id = ""
		
		while True:
			#adds the date for today
			dayObject = datetime.datetime.now()
			dayString = dayObject.strftime("%d%B%Y_%H:%M")
			
			r, w, x = select.select([rfid_device.fd, ardConnection.fileno()], [], [], 0.1)
			#if r means if read
			if r:
				if rfid_device.fd in r:
					for event in rfid_device.read():
						if event.type == ecodes.EV_KEY:
							key_event = categorize(event)
							if key_event.keystate == KeyEvent.key_down:
								if key_event.keycode == 'KEY_KPENTER' or key_event.keycode == 'KEY_ENTER':
									if current_tag_id:
										current_tag_id = current_tag_id.strip(';?').strip()
										print(f"Detected RFID: {current_tag_id}")
										#starts update_mode for adding a tag
										if update_mode:
											update_tag_id = (current_tag_id + "_" + dayString)
											print(f"Adding ID: {update_tag_id} to list")
											authorized_ids.append(update_tag_id)
											write_to_file(authorized_ids, ID_FILENAME)
											#wait 3 seconds for the file to save and close
											time.sleep(3)
											#resets everything
											update_mode = False
											authorized_ids = read_authorized_ids(ID_FILENAME)
											ardConnection.write(b"EXIT_UPDATE\n")
											time.sleep(3)
										else:
											if current_tag_id in authorized_ids:
												print("Access Granted!")
												ardConnection.write(b"OPEN_LOCK\n")
												#waits 3 seconds for arduino response
												time.sleep(3)
												response = ardConnection.readline().decode('ascii').strip()
												print(f"Arduino Response: {response}")
											else:
												print("Access Denied!")
												ardConnection.write(b"NO_ACCESS\n")
									#resets the current tag
									current_tag_id = ""
									#clears the logs
									clear_logs(LOG_FILENAME)
								elif key_event.keycode.startswith('KEY_'):
									key_char = key_event.keycode.replace('KEY_', '')
									if key_char.isdigit():
										current_tag_id += key_char.lower()
									else:
										pass
				#Arduino Message Received
				if ardConnection.fileno() in r:
					try:
						arduino_message = ardConnection.readline().decode('ascii').strip()
						print(f"Arduino sent: {arduino_message}")
						if arduino_message == "UPDATE_MODE":
							print("Arduino wants to change to update mode")
							update_mode = True
							time.sleep(3)
							ardConnection.write(b"UPDATE_READY\n")
						if arduino_message == "DELETE_MODE":
							#deletes the current IDs stored by usingan empty string
							print("Deleting all IDs, and logs")
							emptyArray = []
							write_to_file(emptyArray, ID_FILENAME)
							time.sleep(3)
							write_to_file(emptyArray, LOG_FILENAME)
							ardConnection.write(b"ID_RESET_COMPLETE\n")
							authorized_ids = read_authorized_ids(ID_FILENAME)
							print("IDs and Logs have been deleted")
						else:
							print(f"Unkown message from Arduino {arduino_message}")
					except serial.SerialException:
						print("Arduion Disconnected, will try to reconnect")
						ardConnection.close()
						ardConnection = find_device(ARDUINO_VENDOR_ID, ARDUINO_PRODUCT_ID)
						if ardConnection is None:
							print("Reconnecting failed")
							break
	except FileNotFoundError as f:
		print(f"Error 1 File not found: {f}")
	except PermissionError as p:
		print(f"Error 2 permission error found: {p}")
	except Exception as e:
		print(f"Error 3 Unkown error: {e}")

#starts the program
if __name__ == "__main__":
	main()


