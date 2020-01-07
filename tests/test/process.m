%% set up
close all;
formatSpec = '%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%f%[^\n\r]';
%% LOOP import + display
dif = [];
for t = (99:100:10399)%14599)
    % close
    close all
    
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

    new = getShoreline(grid_new);
    old = getShoreline(grid_orig);
    dif = abs(new-old);
    dif(end+1) = mean(dif);
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