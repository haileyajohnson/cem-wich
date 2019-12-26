%% prep
close all; clear all;

formatSpec = '%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%2f%f%[^\n\r]';

%% import orig verison
filename = 'C:\Users\hailey.johnson\Documents\models\GEE\test_repo\test\output\orig_shoreline.txt';
fileID = fopen(filename,'r');
dataArray = textscan(fileID, formatSpec, 'Delimiter', '', 'WhiteSpace', '', 'TextType', 'string',  'ReturnOnError', false);
fclose(fileID);
origshoreline = [dataArray{1:end-1}];
%% import new version
filename = 'C:\Users\hailey.johnson\Documents\models\GEE\test_repo\test\output\new_shoreline.txt';
fileID = fopen(filename,'r');
dataArray = textscan(fileID, formatSpec, 'Delimiter', '', 'WhiteSpace', '', 'TextType', 'string',  'ReturnOnError', false);
fclose(fileID);
newshoreline = flipud([dataArray{1:end-1}]);
%% Clear temporary variables
clearvars filename formatSpec fileID dataArray ans;

%% display orig
figure()
pcolor(origshoreline);

%% display new
figure()
pcolor(newshoreline)