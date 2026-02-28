%% Jeff Main Valve Test Analysis
% This script is designed to analyze and visualize
% the data collected during the valve actuation test
% of Jeff's main valve on 2-21-26 @10am

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
% Column 1 is relative time (s)
% Column 2 is pressure (psi)

data = readmatrix("JeffMain-test4.csv");
t = data(:,1);
p = data(:,2);

%% Plotting
figure;
ymin = 0;
ymax = 2600;
xmin = 0;
xmax = 360;
lineWidth = 1.25;
axisFontSize = 18;
legendFontSize = 14;
plot(t, p, "LineWidth", lineWidth, "Color", 'b', DisplayName="Pressure (psi)");
hold on;
grid on;
legend(FontSize=legendFontSize, location = "south");
title("WREC: Jeff Valve Stress Test (2-21-26)");
xlabel("Time (s)", fontsize = axisFontSize);
ylabel("Pressure (psi)", FontSize = axisFontSize);
ylim([ymin, ymax]);
xlim([xmin, xmax]);
ax = gca;