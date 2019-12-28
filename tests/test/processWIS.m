daily_hmo = [];
daily_tpd = [];
daily_wavd = [];
daily_sprd = [];
for i = (1:24:size(hmo))
    daily_hmo(end+1) = mean(hmo(i: i+23));
    daily_tpd(end+1) = mean(tpd(i: i+23));
    daily_wavd(end+1) = mean(wavd(i: i+23));
    daily_sprd(end+1) = mean(sprd(i: i+23));
end

mat = horzcat(daily_hmo', daily_tpd', daily_wavd', daily_sprd');
csvwrite('wis63436.txt', mat);