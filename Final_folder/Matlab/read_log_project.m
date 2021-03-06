function [N_SIM, T_SIM, T, data] = read_log_project(filename)
%% Read log file and create simulation variables
% clc; clear all; close all;

temp = importdata(filename);

data = [];
ind = 0;
headers = cell(strrep(temp.textdata,' ',''));
%headers = strsplit(headers{:}, ';');
for i = 1:length(headers)
    ind = ind + 1;
    data.(headers{i}) = temp.data(:,ind);
end

% % store acceleration into an array
% data.acc = [data.acc_x,data.acc_y,data.acc_z];

N_SIM = length(data.time);
T_SIM = 1: N_SIM;
T = 0.016;

end