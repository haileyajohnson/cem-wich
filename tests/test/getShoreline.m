function [shoreline] = getShoreline(mat)
%GETSHORELINE Summary of this function goes here
[~, cols] = size(mat);
shoreline = zeros(1, cols);
for j = 1:cols
    arr = mat(:, j);
    i = find(arr > 0, 1, 'last');
    shoreline(j) = i + mat(i, j);
end

end

