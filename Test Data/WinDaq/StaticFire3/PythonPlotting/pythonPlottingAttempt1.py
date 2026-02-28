import math as m
import matplotlib.pyplot as plt
import numpy as np

# This script is based on one that plotted the pressure data versus time from a csv file
# This version will plot two sets of data on the same plot. One from static fire 3 and one from static fire 2.

# Read the data from the csv file
fileName1 = r'u:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\StaticFire3\PythonPlotting\Trimmed-Data-ForMatlab.csv'
fileName2 = r'u:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\StaticFire2\trimmedCSV\StaticFire2-ForPython.csv'
# The data is in a csv format, so we can use numpy to read it
data1 = np.genfromtxt(fileName1, delimiter=',', skip_header=1)
data2 = np.genfromtxt(fileName2, delimiter=',', skip_header=1)

# Extract the time and pressure data.
# First column is time
# Second column is bottle pressure
# Third column is tank pressure
# Fourth column is chamber pressure
time1 = data1[:, 0]
bottle_pressure1 = data1[:, 1]
tank_pressure1 = data1[:, 2]
chamber_pressure1 = data1[:, 3]

#Same thing for the other file
time2 = data2[:, 0]
bottle_pressure2 = data2[:, 1]
tank_pressure2 = data2[:, 2]
chamber_pressure2 = data2[:, 3]

# Now plot
# set the figure size
plt.figure(figsize=(15, 7))
plt.plot(time1, tank_pressure1, label='Static Fire 3', color='blue')
plt.plot(time2, tank_pressure2, label='Static Fire 2', color='red')

# Set y axis limits from 0 to + 900 PSI
plt.ylim(0, 900)
# Set y axis gridlines to every 100 PSI
plt.yticks(np.arange(0, 901, 100))

# add minor gridlines every 50 PSI
plt.gca().set_yticks(np.arange(0, 901, 50), minor=True)
plt.gca().grid(which='minor', color='gray', linestyle=':', linewidth=0.5)

# Set x limits from 0 to 60 seconds
plt.xlim(0, 60)

# Set x axis gridlines to every 10 seconds
plt.xticks(np.arange(0, 61, 10))

# add minor gridlines every 5 seconds
plt.gca().set_xticks(np.arange(0, 61, 5), minor=True)
plt.gca().grid(which='minor', color='gray', linestyle=':', linewidth=0.5)

# Add formatting
plt.title('Tank Pressure vs Time')
plt.xlabel('Time (s)')
plt.ylabel('Pressure (psi)')
plt.legend()
plt.grid()



# Show the plot
# Save the plot to a file
#plt.savefig(r'u:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\StaticFire3\PythonPlotting\TankPressureVsTime.png', dpi=400, bbox_inches='tight')
#plt.show()

# Make a new figure that also shows bottle pressure with the elapsed time.
# set the figure size
plt.figure(figsize=(15, 7))
# Static fire 3 bottle and tank pressure and chamber pressure
plt.plot(time1, tank_pressure1, label='3 Tank Pressure', color='darkblue')
plt.plot(time1, bottle_pressure1, label='3 Bottle Pressure', color='lightblue')
plt.plot(time1, chamber_pressure1, label='3 Chamber Pressure', color='cornflowerblue')

# Static fire 2 bottle and tank pressure and chamber pressure
plt.plot(time2, tank_pressure2, label='2 Tank Pressure', color='darkred')
plt.plot(time2, bottle_pressure2, label='2 Bottle Pressure', color='lightcoral')
plt.plot(time2, chamber_pressure2, label='2 Chamber Pressure', color='indianred')

# Set y axis limits from 0 to + 900 PSI
plt.ylim(0, 1000)

# Set y axis gridlines to every 100 PSI
plt.yticks(np.arange(0, 1001, 100))

# add minor gridlines every 50 PSI
plt.gca().set_yticks(np.arange(0, 1001, 50), minor=True)
plt.gca().grid(which='minor', color='gray', linestyle=':', linewidth=0.5)

# Set x limits from 0 to 60 seconds
plt.xlim(0, 60)

# Set x axis gridlines to every 10 seconds
plt.xticks(np.arange(0, 61, 10))

# add minor gridlines every 5 seconds
plt.gca().set_xticks(np.arange(0, 61, 5), minor=True)
plt.gca().grid(which='minor', color='gray', linestyle=':', linewidth=0.5)

# Add formatting
plt.title('Static Fire 2 and 3 fill comparisons')
plt.xlabel('Time (s)')
plt.ylabel('Pressure (psi)')
plt.legend()
plt.grid()

# Save the plot to a file
# plt.savefig(r'u:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\StaticFire3\PythonPlotting\FillComparison.png', dpi=400, bbox_inches='tight')

# Show the plot
plt.show()