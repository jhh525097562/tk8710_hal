
tt = 0:8191;
x1 = sum(Hhat(1,1)*sin(pi*tt/8191).*exp((1i)*2*pi*976.6*tt/125000));
x2 = sum(Hhat(1,1)*exp((1i)*2*pi*976.6*tt/125000));
y = x1/x2