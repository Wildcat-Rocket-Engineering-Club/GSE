import math as m
import matplotlib.pyplot as plt
import numpy as np

# This script will plot the pressure data versus time from a csv file

# Read the data from the csv file
fileName = r'u:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\StaticFire3\PythonPlotting\Trimmed-Data-ForMatlab.csv'
# The data is in a csv format, so we can use numpy to read it
data = np.genfromtxt(fileName, delimiter=',', skip_header=1)

# Extract the time and pressure data.
# First column is time
# Second column is bottle pressure
# Third column is tank pressure
# Fourth column is chamber pressure
time = data[:, 0]
bottle_pressure = data[:, 1]
tank_pressure = data[:, 2]
chamber_pressure = data[:, 3]

# Just plot the tank pressure versus time
plt.plot(time, tank_pressure, label='Static Fire 3', color='blue')
# Add formatting
plt.title('Tank Pressure vs Time')
plt.xlabel('Time (s)')
plt.ylabel('Pressure (psi)')
plt.legend()
plt.grid()
# Show the plot
plt.show()
