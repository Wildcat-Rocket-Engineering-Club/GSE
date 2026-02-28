# StaticFire4_ThrustImpulseAnalysis
# This code takes the data given by WREC Far-Out Fire 2
# We have time (s) and pressure for the bottle, tank, and chamber (PSI), in that order.

# Import libraries for plotting and calculating.
# Removed unused import of math library
import matplotlib.pyplot as plt
import numpy as np
import os

saveImages = True

showFigure1 = True
showFigure2 = True
showFigure3 = True



# Read the data from the file:
fileName1 = r'u:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\Far-Out\FAR-Fire3-Trimmed.csv'
data1 = np.genfromtxt(fileName1, delimiter=',', skip_header=1)

# Extract the time and pressure data.
# First column is time
# Second column is bottle pressure
# Third column is tank pressure
# Fourth column is chamber pressure
time1 = data1[:, 0]
bottle_pressure1 = data1[:, 1]
tank_pressure1 = data1[:, 2]
chamber_pressure1 = data1[:, 3]

# Make a new figure that also shows bottle pressure with the elapsed time.
# set the figure size
plt.figure(figsize=(15, 7))
# Static fire 3 bottle and tank pressure and chamber pressure
plt.plot(time1, tank_pressure1, label='Tank Pressure', color='green')
plt.plot(time1, bottle_pressure1, label='Bottle Pressure', color='lightblue')
plt.plot(time1, chamber_pressure1, label='Chamber Pressure', color='red')

yMax = 1200
# Set y axis limits from 0 to + 1200 PSI
plt.ylim(0, yMax)

# Set y axis gridlines to every 100 PSI
plt.yticks(np.arange(0, yMax+1, 100))

# add minor gridlines every 50 PSI
plt.gca().set_yticks(np.arange(0, 1001, 50), minor=True)
plt.gca().grid(which='minor', color='gray', linestyle=':', linewidth=0.5)

#Set x limits from 1900 to 2116 seconds
plt.xlim(1996, 2106)

# Set x axis gridlines to every 5 seconds
plt.xticks(np.arange(1996, 2106, 10))

## add minor gridlines every 5 seconds
#plt.gca().set_xticks(np.arange(0, 61, 5), minor=True)
#plt.gca().grid(which='minor', color='gray', linestyle=':', linewidth=0.5)

# Add formatting
plt.title('FAR-OUT Fire 2 data')
plt.xlabel('Time (s)')
plt.ylabel('Pressure (psi)')
plt.legend()
plt.grid()

# Save the plot to a file
# if a prior version exists, make another version
# use a loop to find out what file names exist
# and update this image name to the next version
fileName = "EverythingGraph"
filePath = "U:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\Far-Out\Graphs\WholeThing"
output_file = os.path.join(filePath, fileName)
i = 1
while True:
    fileName = output_file + str(i) + '.png'
    try:
        with open(fileName, 'r') as f:
            i += 1
    except FileNotFoundError:
        break

if(saveImages):
    plt.savefig(fileName, dpi=400, bbox_inches='tight')

# Show the plot
if(showFigure1):
    plt.show()


# New plot for the thrust/impulse.
# 75-85 seconds for the actual firing.
dt = 0.004
t0 = 2095
tf = 2101

# Plot just the chamber presssure between 2095 and 2101 seconds.
plt.figure(figsize=(15, 7))
plt.plot(time1, chamber_pressure1, label='Chamber Pressure', color='red')
plt.plot(time1, tank_pressure1, label='Tank Pressure', color='green')
pMax_Plot = 900
plt.xlim(t0, tf)
plt.ylim(0, pMax_Plot)
plt.title('FAR-OUT Fire 2')
plt.ylabel('Pressure (psi)')
plt.xlabel('Time (s)')
# Set y axis gridlines to every 50 PSI
plt.yticks(np.arange(-50, pMax_Plot + 1, 50))
# add minor gridlines every 10 PSI
#plt.gca().set_yticks(np.arange(0, pMax_Plot + 1, 10), minor=True)
# Set x axis gridlines to every 1 second
plt.xticks(np.arange(t0, tf , 1))
# add minor gridlines every 0.2 seconds
plt.gca().set_xticks(np.arange(t0, tf + 1, 0.2), minor=True)
plt.gca().grid(which='minor', color='gray', linestyle=':', linewidth=0.5)
plt.gca().grid(which='major', color='gray', linestyle='-', linewidth=0.5)
plt.legend()

# Save the figure to a file
# if a prior version exists, make another version
# use a loop to find out what file names exist
# and update this image name to the next version
fileName = "BurnGraph"
filePath = "U:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\Far-Out\Graphs\Burn"
output_file = os.path.join(filePath, fileName)

i = 1
while True:
    fileName = output_file + str(i) + '.png'
    try:
        with open(fileName, 'r') as f:
            i += 1
    except FileNotFoundError:
        break
if(saveImages):
    plt.savefig(fileName, dpi=400, bbox_inches='tight')
# show
if(showFigure2):
    plt.show()

# Find the start and end of the burn.
# Let's say the start is 76.8 seconds and the end is 79.6 seconds
# Set them as t0-burn and tf-burn
# Ideally we do this by numerically finding when the chamber pressure spikes after the initial 150PSI spike
# then find when the pressure drops off fast.


# BURN START CHANGED AT 5-10-25 4:30PM
# originally: 76.8
# now: 76.6
t0_burn = 2096.1
tf_burn = 2098.6
burn_time = tf_burn - t0_burn

# find the index of the start and end of the burn
t0_index = np.where(time1 == t0_burn)[0][0]
tf_index = np.where(time1 == tf_burn)[0][0]

# find the time and pressure data for the burn
time_burn = time1[t0_index:tf_index]
chamber_pressure_burn = chamber_pressure1[t0_index:tf_index]

# Throat area is 0.785 in^2
throat_area = 0.785

# Assume a thrust coefficicent of 1.3
thrust_coefficient = 1.3

# Calculate the total impulse by taking trapezoidal rule of burn chamber pressure and burn time
impulse = 0

thrust_lbf = chamber_pressure_burn * throat_area * thrust_coefficient

impulse = np.trapezoid(thrust_lbf, dx=dt)

# Calculate thrust in Newtons. Turns lbf to N
thrust_newtons = thrust_lbf * 4.44822

# Calculate cumulative impulse over time
cumulative_impulse_newtons = np.zeros(len(time_burn))
for i in range(len(time_burn)):
    cumulative_impulse_newtons[i] = np.trapezoid(thrust_newtons[:i + 1], dx=dt)

# function to print a line
def pl():
    print("-"*50)

# Grab the starting and ending tank pressure
initial_tank_pressure = tank_pressure1[t0_index]
final_tank_pressure = tank_pressure1[tf_index]
delta_tank_pressure = final_tank_pressure - initial_tank_pressure

# Plot the burn thrust over time
nSigFigs = 2

print("FAR-OUT Fire 2 Thrust and Impulse Analysis")
pl()
print("Thrust Coefficient: ", thrust_coefficient)
print("Throat Area: ", throat_area, "in^2")
pl()
# Print the burn time
print('Burn Time: ', np.round(burn_time, nSigFigs), 's')
pl()

# Impulse is in lbf-s
print('Total Impulse: ', np.round(impulse, nSigFigs), 'lbf-s')

# Convert impulse to N-s
impulse = impulse * 4.44822
print('Total Impulse: ', np.round(impulse, nSigFigs), 'N-s')
pl()

# Calculate the thrust by dividing the impulse by the burn time
avg_thrust = impulse / burn_time

print('Average Thrust: ', np.round(avg_thrust, nSigFigs), 'N')

# Calculate the average thrust in lbf
avg_thrust_lbf = avg_thrust / 4.44822
print('Average Thrust: ', np.round(avg_thrust_lbf, nSigFigs), 'lbf')

pl()

# max thrust in lbf
max_thrust_lbf = max(thrust_lbf)
print('Max Thrust: ', np.round(max_thrust_lbf, nSigFigs), 'lbf')

# max thrust in newtons
max_thrust_newtons = max(thrust_newtons)
print('Max Thrust: ', np.round(max_thrust_newtons, nSigFigs), 'N')

pl()

# Create a figure with two subplots side-by-side
fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 7), gridspec_kw={'width_ratios': [2.75, 1]})

# Plot the thrust over time in the first subplot
ax1.plot(time_burn, thrust_newtons, label='Thrust', color='blue')
ax1.set_xlim(t0_burn, tf_burn)
ax1.set_title('Far-Out Fire 2 Thrust')
ax1.set_ylabel('Thrust (N)')
ax1.set_ylim(900, max(thrust_newtons) + 50)
ax1.set_xlabel('Time (s)')
ax1.grid()
ax1.legend(loc='lower left')

# Set major gridlines every 100 Newtons
ax1.set_yticks(np.arange(900, max(thrust_newtons) + 0, 100))


# Add dashed line for average thrust
ax1.axhline(y=avg_thrust, color='red', linestyle='--', label='Average Thrust')

# add a label on the graph to state thrust
ax1.text(tf_burn - 0.5, avg_thrust + 10, f"Avg Thrust: {np.round(avg_thrust, nSigFigs)} N", color='red', fontsize=10, ha='right')

# Define the data for the table
table_data = [
    ["Total Impulse", f"{str(np.round(impulse, nSigFigs)) + ' N-s'}"],
    ["Average Thrust", f"{str(np.round(avg_thrust, nSigFigs)) + ' N'} "],
    ["Average Thrust", f"{str(np.round(avg_thrust_lbf, nSigFigs)) + ' lbf'}"],
    ["Peak Thrust", f"{str(np.round(max(thrust_newtons), nSigFigs)) + ' N'}"],
    ["Peak Thrust", f"{str(np.round(max(thrust_lbf), nSigFigs)) + ' lbf'}"],
    ["Initial Tank Pressure", f"{str(np.round(initial_tank_pressure, nSigFigs)) + ' PSI'}"],
    ["Final Tank Pressure", f"{str(np.round(final_tank_pressure, nSigFigs)) + ' PSI'}"],
    ["Tank Pressure Used", f"{str(np.round(delta_tank_pressure, nSigFigs))+ ' PSI'}"],
    ["Burn Time", f"{str(np.round(burn_time, nSigFigs)) + ' s'}"],
    ["Throat Area", f"{str(throat_area) + ' in^2'}"],
    ["Thrust Coefficient", f"{str(thrust_coefficient)}"]
]

# Add the table to the second subplot
ax2.axis('off')  # Turn off the axis for the table
table = ax2.table(
    cellText=table_data,
    colLabels=None,
    cellLoc='center',
    loc='center',
    cellColours=[['lightgrey'] * 2] * len(table_data)
)
table.auto_set_font_size(False)
table.set_fontsize(10)
table.scale(1.2, 2)  # Scale the table for better readability


# Save the figure to a file
# if a prior version exists, make another version
# use a loop to find out what file names exist
# and update this image name to the next version
fileName = "ThrustImpulseGraph"
filePath = "U:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\Far-Out\Graphs\ThrustImpulse"
output_file = os.path.join(filePath, fileName)
i = 1
while True:
    fileName = output_file + str(i) + '.png'
    try:
        with open(fileName, 'r') as f:
            i += 1
    except FileNotFoundError:
        break
if(saveImages):
    plt.savefig(fileName, dpi=400, bbox_inches='tight')
# show
if(showFigure3):
    plt.show()


# Convert the thrust data
# Remember:
# time_burn = time1[t0_index:tf_index]
# chamber_pressure_burn = chamber_pressure1[t0_index:tf_index]

new_dt = 80/1000
iStep = round(new_dt / dt)
new_time = time_burn[::iStep] - t0_burn
new_Thrust = thrust_newtons[::iStep]

# Round the new_time and new_Thrust to 3 significant digits
new_time = np.round(new_time, 3)
new_Thrust = np.round(new_Thrust, 3)

# Save the thrust vs time data to a new csv file
# only do this if the file doesn't exist already.
t_ms = new_dt * 1000
fileName = "ThrustVsTime_"+str(t_ms)+"ms_Resolution.csv"
filePath = "U:\Documents\Documents\School\Clubs\WREC\Data\WinDaq\Far-Out\ThrustCurve_CSVs"
output_file = os.path.join(filePath, fileName)
if not os.path.exists(output_file):
    with open(output_file, 'w') as f:
        f.write("Time (s),Thrust (N)\n")
        for t, thrust in zip(new_time, new_Thrust):
            f.write(f"{t},{thrust}\n")
    print("File created successfully!")
else:
    print("Thrust curve exists already!")