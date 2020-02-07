function [new_angles] = azmToShoreNormal(shore_normal,angles,form)

deg = 0;
if (nargin > 2 && strcmp(form, 'deg'))
    deg = 1;
end

if deg
    angles = deg2rad(angles);
    shore_normal = deg2rad(shore_normal);
end
    
new_angles = angles - shore_normal;

while(any(new_angles < -pi))
    new_angles(new_angles < -pi) = new_angles(new_angles < -pi) + (2*pi);
end

while(any(new_angles > pi))
    new_angles(new_angles > pi) = new_angles(new_angles > pi) - (2*pi);
end

if deg
    new_angles = rad2deg(new_angles);
end

end

