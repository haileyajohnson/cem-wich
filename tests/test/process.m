%% set up
close all; clear all;
formatSpec = '%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%9f%f%[^\n\r]';
%% LOOP import + display
for t = (0:1:364)
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
    
    dif_grid = grid_orig - grid_new;
%     if abs(sum(sum(dif_grid))) > 0.01
%         figure(1)
%         pcolor(grid_orig);
%         shading flat; axis equal;
% 
%         figure(2)
%         pcolor(grid_new);
%         shading flat; axis equal;
% 
%         figure(3)
%         pcolor(grid_orig - grid_new);
%         shading flat; axis equal;

%         break;
%     end
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