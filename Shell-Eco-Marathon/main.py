
# Shell Eco-Marathon 2018 Raspberry Pi Main Code
# Volts-Wagon user interface. Handles buttons, enables, and is the i2c master
# for the system. Made to use with HDMI LCD.
print("no libraries")
import pygame
from pygame.locals import *
import time
import smbus
import sys
import struct
import RPi.GPIO as GPIO
print("imported all libraries")
# use smbus for i2c
bus = smbus.SMBus(1)
time.sleep(5)
# use GPIO board mode
GPIO.setmode(GPIO.BOARD)
GPIO.setup(7, GPIO.IN, pull_up_down = GPIO.PUD_UP) # speed up
GPIO.setup(11, GPIO.IN, pull_up_down = GPIO.PUD_UP) # speed down
GPIO.setup(12, GPIO.IN, pull_up_down = GPIO.PUD_UP) # reset timer
GPIO.setup(16, GPIO.IN) # motor enable wire (dead-man switch), pin 3 on teensy
# screen size
size = (640,480)

# this is the address we setup in the Arduino Program
teensy_address = 0x03
nano_address = 0x04
print("Preinit")
# setup for pygame GUI
pygame.init()
print("init done")
screen = pygame.display.set_mode(size,pygame.FULLSCREEN)

pygame.display.set_caption("Volts-Wagon User Interface")

pygame.key.set_repeat()

def readBytes(address):
    # read 12 bytes for i2c
    dbytes = bus.read_i2c_block_data(address, 0, 12)
    return dbytes;

def writeBytes(address, value):
    # write 12 bytes to i2c
    bus.write_i2c_block_data(address, 0, value)
    return -1

def getfloat(dbytes, i):
    # convert received 12 bytes to array of floats
    db = dbytes[4*i:(i+1)*4]    
    return struct.unpack('f', bytes(db))[0]

def showUI(driverScreen, speed, dspeed, voltage, efficiency, timestr):
    # display driver GUI with above variables + time   
    # fill the screen with a black background
    driverScreen.fill((0,0,0))
    
    # Define some fonts to draw text with
    font = pygame.font.SysFont(None, 75)
    fontsmall = pygame.font.SysFont(None, 50)

    speedstr = ("SPD: {:5.1f} mph").format(speed)
    dspeedstr = ("SET: {:5.1f} mph").format(dspeed)
    voltstr = ("BATT: {0} V").format(voltage)
    efficiencystr = ("EFF: {0} mi/kWh").format(efficiency)

    # render
    speedLabel = font.render(speedstr, 1, (255,255,0))
    dspeedLabel = font.render(dspeedstr, 1, (255, 255, 150))
    voltLabel = font.render(voltstr, 1, (255,0,0))
    efficiencyLabel = font.render(efficiencystr, 1, (0,255,0))
    timeLabel = font.render(timestr, 1, (255, 255, 255))

    textpos = (20, 50) # speed position
    textpos1 = (textpos[0], textpos[1]+50) # desired speed position
    textpos2 = (textpos1[0], textpos1[1]+50) # voltage
    textpos3 = (textpos[0], textpos[1]-50) # efficiency
    textposTime = (textpos2[0], textpos2[1]+50) # time
    
    # draw the text onto our screen
    driverScreen.blit(voltLabel, textpos2)
    driverScreen.blit(dspeedLabel, textpos1)
    driverScreen.blit(speedLabel, textpos)
    driverScreen.blit(efficiencyLabel, textpos3)
    driverScreen.blit(timeLabel, textposTime)
    
    # update the display
    pygame.display.flip()

# set up variables for main loop
refresh = 0
dspeed = 0
speed = 0
power = 0
voltage = 0
efficiency = 0
start = 0
motorFlag = 0
# main loop
while True:
    try:
        # speed control buttons
        if not GPIO.input(7) and dspeed < 20:
            dspeed = dspeed + .5
            time.sleep(.1)
        if not GPIO.input(11) and dspeed > 0:
            dspeed = dspeed - .5
            time.sleep(.1)
        motorFlag = getfloat(readBytes(nano_address), 1)
        if motorFlag == 0: # motor enable wire 1 = motor on
           print("motor on")
           rpmspeed = getfloat(readBytes(nano_address), 0)
           speed = (60*2*3.14159265*10*rpmspeed)/(12*5280) # use hall effect spd
        else:
           #speed = getfloat(readBytes(teensy_address), 0) # use gps speed
           rpmspeed = getfloat(readBytes(nano_address), 0)
       
           speed = (60*2*3.14159265*10*rpmspeed)/(12*5280)
           print("motor off")
           
        time.sleep(.01)
        #power = getfloat(readBytes(teensy_address), 1)
        time.sleep(.01)
        #voltage = getfloat(readBytes(teensy_address), 2)
        time.sleep(.01)
        
        # send desired speed to arduino nano
        if dspeed != 0:
            dspeedrpm = (12*5280*dspeed)/(60*2*3.14159265*10) # dspeed in rpm
        else:
            dspeedrpm = 0 # avoid zero division error

        # reset desired speed to 0 if dead-man switch triggered
        if motorFlag == 1:
            dspeedrpm = 0
            dspeed = 0
            
        send = struct.pack('f', dspeedrpm) # pack desired speed for i2c

        # arduino nano expects 12 bytes, must pad i2c send array
        sendarray = [send[0],send[1],send[2],send[3],0,0,0,0,0,0,0,0]
        writeBytes(nano_address, sendarray)
    except IOError: #i2c gets angry if arduino is silent
        pass
        
    # stopwatch timer
    if not GPIO.input(12):
        start = time.time()
        
    now = time.time() - start # time in seconds since the reset button was pressed
    # calculate h,m,s
    m,s = divmod(now,60)
    h,m = divmod(m,60)
    # format timer
    msg= "TIME: %02d:%02d" % (m,s)
    psec = str(now-int(now))
    pstr = psec[1:5]
    msg = msg + str(pstr)

    miles = speed*(now / 3600)
    if power != 0:
        energy = power * 1000 * now / 3600
        efficiency = miles/energy # efficiency in miles / kilowatt-hour
    else:
        energy = 0
        efficiency = 0 # case for no data available to prevent zero division
    
    if pygame.time.get_ticks() > refresh:
        
        # run the function to update display      
        showUI(screen, speed, dspeed, voltage, efficiency, msg)
        
        # update refresh time to 100ms in the future
        refresh = pygame.time.get_ticks() + 75

    # exit loop
    for event in pygame.event.get():
        if event.type == KEYDOWN:
            pygame.quit()
