# ------------------------------------------------------------------------------------------------------------------------------------
#
# This code seems to turn a csv file of thrust and time into a file usable for simulators
# it follows the format given here:
# https://www.thrustcurve.org/info/raspformat.html
#
# ------------------------------------------------------------------------------------------------------------------------------------
#
# Beware, some older simulation softwares will only accept a max of 32 data points...
#
# ------------------------------------------------------------------------------------------------------------------------------------
# 
# There's a bunch of info about the motor size, class, delays, etc.
# that come first, and THEN goes the thrust vs time data
#
# ------------------------------------------------------------------------------------------------------------------------------------
#
# Author: Etan Grant
# Date: 5-10-2025
#
# ------------------------------------------------------------------------------------------------------------------------------------

# import libraries
import matplotlib.pyplot as plt
import numpy as np
import os
from datetime import datetime

# function to print a line
def pl():
    print("-"*50)

print()
pl()

# ------------------------------------------------------------------------------------------------------------------------------------

# INPUT VALUES:

motorName = "Mojave Sphinx"

author = "Etan Grant"

givenImpulse = 5512.5 # Reference... Calculated impulse from the thrustImpulseAnalysis python file

diameterInches = 4 # motor diameter

caseLengthInches = 12 # motor length

propMass_lbs = 7.78888248 # propellant mass (INCLUDE FUEL AND OXIDIZER IN THIS!)

totalMass_lbs = propMass_lbs # total motor mass. For liquid motors, we don't consider engine mass, so it's equal to prop mass.

hasDelay = False # Should be zero for a liquid motor

delays = [-1, -1, -1] # Liquid motors don't have delays... BUT if you did, you can make an array of delays charges here (in seconds)

manufacturerName = "UA" # Manufacturer name abbreviation. Check the name format here: https://www.nar.org/docs.ashx?id=1468138

shorthandname = 'ThrustVsTime_100.0ms_Resolution-Gas' # Source CSV file name

sourcePath = 'U:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\Far-Out\ThrustCurve_CSVs-Fire3' # Source folder

savePathStr = "U:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\Far-Out\simfiles" # Output folder

# ------------------------------------------------------------------------------------------------------------------------------------

# Combine the user-provided folder location with the shorthand name to create the full file path
fileName = os.path.join(sourcePath, shorthandname + '.csv')  # source data location

# or calculated directly like so
# Load the CSV file

data = np.genfromtxt(fileName, delimiter=',', skip_header=1)

# Extract time and thrust columns
time = data[:, 0]
thrust = data[:, 1]

# Calculate total impulse using the trapezoidal rule
impulse = np.trapezoid(thrust, time)
print("Given Impulse: "+str(round(givenImpulse, 2))+" N-s")
print("File's Impulse: "+str(round(impulse, 2))+" N-s")
pl()

# ------------------------------------------------------------------------------------------------------------------------------------

# Determine motor class (should be type L)
if impulse < 1.26:
    motorClass = "Below A"
    print("Warning: Motor class could be out of range.")
elif impulse <= 2.5:
    motorClass = "A"
elif impulse <= 5.0:
    motorClass = "B"
elif impulse <= 10.0:
    motorClass = "C"
elif impulse <= 20.0:
    motorClass = "D"
elif impulse <= 40.0:
    motorClass = "E"
elif impulse <= 80.0:
    motorClass = "F"
elif impulse <= 160.0:
    motorClass = "G"
elif impulse <= 320.0:
    motorClass = "H"
elif impulse <= 640.0:
    motorClass = "I"
elif impulse <= 1280.0:
    motorClass = "J"
elif impulse <= 2560.0:
    motorClass = "K"
elif impulse <= 5120.0:
    motorClass = "L"
else:
    motorClass = "M"
    print("Warning: Motor class could be out of range.")
print("Motor Class: "+motorClass)

pl()

# ------------------------------------------------------------------------------------------------------------------------------------

# Find the average thrust (impulse/burn time)
# Find the burn time by determining the duration where thrust is greater than zero
burnTime = time[thrust > 0].max() - time[thrust > 0].min()
burnTime_str = str(round(burnTime, 1))
print("Burn Time: "+burnTime_str+" s")
avgThrust = impulse/burnTime
pl()

# Calculate average thrust
avgThrust_str = str(int(round(avgThrust, 0)))
print("Average Thrust: "+avgThrust_str+ " N")
pl()

# ------------------------------------------------------------------------------------------------------------------------------------

# turn the diameter into mm
diameterMillimeters = diameterInches*25.4
diameterMillimeters = int(round(diameterMillimeters, 0))
motorDiameter_str = str(diameterMillimeters)
print("Motor diameter: "+motorDiameter_str+" mm")
pl()

# Next, case length into mm
lengthMillimeters = caseLengthInches*25.4
lengthMillimeters = int(round(lengthMillimeters, 0))
caseLengthStr = str(lengthMillimeters)
print("Casing Length: "+caseLengthStr+" mm")
pl()

# ------------------------------------------------------------------------------------------------------------------------------------

# Next: delays
if(not(hasDelay)):
    # Liquid motors don't have a delay charge (they're liquid motors)
    # so you gotta write it like this...?
    delayStr = "P"
else:
    delayStr = ""
    for delay in delays:
        delayStr = delayStr+str(int(delay))
print("Delays: "+delayStr)
pl()

# ------------------------------------------------------------------------------------------------------------------------------------

# Next: Propellant weight, in kg (erm it's actually mass)
propMass = propMass_lbs / 2.205
propMass = round(propMass, 3)
propMassStr = str(propMass)
print("Propellant Weight: "+propMassStr+" kg")
pl()

# Next: Total weight, in kg (erm!!!!!)
totalMass = totalMass_lbs / 2.205
totalMass = round(totalMass,3)
totalMassStr = str(totalMass)
print("Total Mass: "+totalMassStr + " kg")
pl()

# ------------------------------------------------------------------------------------------------------------------------------------

# Next: date
# Get the current date
current_date = datetime.now()

# Format the date as MM/YYYY
date_str = current_date.strftime("%-m/%Y") if os.name != 'nt' else current_date.strftime("%#m/%Y")
print("Date: " + date_str)
pl()

# ------------------------------------------------------------------------------------------------------------------------------------

# Next: manufacturer (last one!)
print("Manufacturer: "+manufacturerName)
pl()


# Replace spaces in the manufacturer name and motor name with underscores.
manufacturerName = manufacturerName.replace(" ", "_")
motorName = motorName.replace(" ", "_")

# ------------------------------------------------------------------------------------------------------------------------------------

# And just to add to the file name, let's find the (avg) resolution in ms
# Calculate the average time between points in milliseconds
time_differences = np.diff(time)  # Find the differences between consecutive time points
avg_time_ms = np.mean(time_differences) * 1000  # Convert to milliseconds
avg_time_ms= round(avg_time_ms, 1)
avg_time_ms = int(avg_time_ms)
avg_time_str = str(avg_time_ms)
print("Average Time Between Points: " + avg_time_str + " ms")
avg_time_str = avg_time_str+"ms_resolution"
pl()

# ------------------------------------------------------------------------------------------------------------------------------------

#  Finally: We create the lines of this thing according to https://www.thrustcurve.org/info/raspformat.html
# Comment Lines with semicolons... Create a new file!!!
outputName = manufacturerName + "_" + motorName + "_" + motorClass+avgThrust_str +"_"+avg_time_str+ ".eng"

# print("File name: "+outputName)
outputPath = savePathStr + "\\" + outputName

try:
    if os.path.exists(outputPath):
        raise FileExistsError(f"The file '{outputName}' already exists. Please choose a different name or delete the existing file.")
    
    with open(outputPath, 'w') as file:
        file.write("; " + motorName + " " + motorClass + avgThrust_str + "\n")
        file.write("; Created by " + author + " " + date_str + "\n")
        file.write(motorClass+avgThrust_str+" "+motorDiameter_str+" "+caseLengthStr+" "+delayStr+" "+propMassStr+" "+totalMassStr+" "+manufacturerName+"\n")
        for i in range(len(time)):
            file.write("   "+str(time[i])+" "+str(thrust[i])+"\n")
        file.write(";"+"\n")
    print("File successfully created: " + outputName)
except FileExistsError as fe:
    print(str(fe))
except Exception as e:
    print("An error occurred while creating the file: " + str(e))