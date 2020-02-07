close all; clear all;

shore_norm = 20;

waves = deg2rad((rand(1, 100) * 180) + (shore_norm - 90));

figure();
pax = polaraxes;
polarhistogram(waves, 15);%, deg2rad(0:10:360));
pax.ThetaDir = 'clockwise';
pax.ThetaZeroLocation = 'top';
title('Azimuth');
pax.RTickLabel = [];
hold on
polarplot([deg2rad(shore_norm), deg2rad(shore_norm)], [0, 15], 'r', 'linewidth', 2);

new_waves = azmToShoreNormal(deg2rad(shore_norm), waves);

figure()
pax = polaraxes;
polarhistogram(new_waves, 15) %, deg2rad(-180:10:180));
pax.ThetaDir = 'clockwise';
pax.ThetaZeroLocation = 'top';
title('Shore Normal');
pax.RTickLabel = [];
pax.ThetaTickLabel = [(0:30:180), (-150:30:-30)];
hold on
polarplot([0, 0], [0, 15], 'r', 'linewidth', 2);