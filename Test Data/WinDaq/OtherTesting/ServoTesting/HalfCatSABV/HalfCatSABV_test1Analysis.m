%% HalfCat SABV Test Analysis
% This script is designed to analyze and visualize
% the data collected during the valve actuation test
% of the HalfCat main valve (#1) on 2-21-26 @9:30ish am

%% Housekeeping
% clear stuff
close all;
clc;
clear;

% Set interpereter to Latex.
set(groot, 'defaultTextInterpreter', 'latex');
set(groot, 'defaultAxesTickLabelInterpreter', 'latex');
set(groot, 'defaultLegendInterpreter', 'latex');

%% Load Data
% Column 1 is relative time (s)
% Column 2 is pressure (psi)

data = readmatrix("HalfCatSABV-test2.csv");
t = data(:,1);
p = data(:,2);

%% Plotting
figure;
ymin = 0;
ymax = 2600;
xmin = 7;
xmax = 87;
lineWidth = 1.25;
axisFontSize = 18;
legendFontSize = 14;
plot(t, p, "LineWidth", lineWidth, "Color", 'b', DisplayName="Pressure (psi)");
hold on;
grid on;
legend(FontSize=legendFontSize, location = "south");
title("WREC: HalfCat SABV Test 1 (2-21-26)");
xlabel("Time (s)", fontsize = axisFontSize);
ylabel("Pressure (psi)", FontSize = axisFontSize);
% ylim([ymin, ymax]);
xlim([xmin, xmax]);
ax = gca;