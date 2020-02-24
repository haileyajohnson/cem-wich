%% set up
close all;
% 26x28
% formatSpec = '%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%f%[^\n\r]';
% 103x110
%formatSpec = '%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%f%[^\n\r]';
% 100x300
% formatSpec = '%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%f%[^\n\r]';
% 50x100
formatSpec = '%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%f%[^\n\r]';


%% LOOP import + display
difs = [];
for t = (0:1:(365*1-1))
    % close
    
    % Import the datas
    filename = sprintf("output/orig/CEM_%06d.out", t);
    fileID = fopen(filename,'r');
    data = textscan(fileID, formatSpec, 'Delimiter', '', 'WhiteSpace', '', 'TextType', 'string',  'ReturnOnError', false);
    grid_orig = [data{1:end-1}];
    fclose(fileID);
    
    filename = sprintf("output/new/CEM_%06d.out", t);
    fileID = fopen(filename,'r');
    data = textscan(fileID, formatSpec, 'Delimiter', '', 'WhiteSpace', '', 'TextType', 'string',  'ReturnOnError', false);
    grid_new = flipud([data{1:end-1}]);
    fclose(fileID);

%     new = getShoreline(grid_new);
%     old = getShoreline(grid_orig);
%     dif = abs(new-old);
%     difs(end+1) = mean(dif); %length(find(dif >= 1));

    dif = grid_new - grid_orig;
    figure(1)
    pcolor(dif);
    shading flat; axis equal;
    pause(0.2)
end

figure(1)
pcolor(grid_orig);
shading flat; axis equal;

figure(2)
pcolor(grid_new);
shading flat; axis equal;

figure(3)
pcolor(grid_orig - grid_new);
shading flat; axis equal;