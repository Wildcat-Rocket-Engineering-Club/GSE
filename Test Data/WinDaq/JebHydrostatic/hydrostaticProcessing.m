%% Housekeeping
% clear stuff
close all;
clc;
clear;

% Set interpereter to Latex.
set(groot, 'defaultTextInterpreter', 'latex');
set(groot, 'defaultAxesTickLabelInterpreter', 'latex');
set(groot, 'defaultLegendInterpreter', 'latex');


%% Data
% Column 1 is relative time
% Column 2 is Date
% Column 3 is Time Stamp UTC
% Column 4 is psi

data = readmatrix("hydrostatic1.csv");
t = data(:,1);
t_min = t/60;
t_hrs = t_min/60;
p = data(:,4);

%% Plotting
figure;
ymin = 1400;
ymax = 1600;
xmin = 10;
xmax = xmin+60;
lineWidth = 1.25;
axisFontSize = 18;
legendFontSize = 14;
plot(t_min, p, "LineWidth", lineWidth, "Color", 'b', DisplayName="Pressure (psi)");
hold on;
grid on;
legend(FontSize=legendFontSize, location = "south");
xlabel("Time (min)", fontsize = axisFontSize);
ylabel("Pressure (psi)", FontSize = axisFontSize);
ylim([ymin, ymax]);
xlim([xmin, xmax]);
ax = gca;