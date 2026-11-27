function gt = geth(fs)

    T=1;
    B=100;%仿真BT=0.38.B=∞时GMSK变为MSK
    h=0.5;
    L=3;
    t=0:1:fs*L-1;
    bata=pi*B*sqrt(2/log(2));%计算β值
    t1=bata*(t/fs-(L-1)*T/2);
    t2=bata*(t/fs-(L+1)*T/2);

    q=1/(2*bata*T)*(t2.*erfc(t2)-t1.*erfc(t1)-1/sqrt(pi)*exp(-(t2).^2)+1/sqrt(pi)*exp(-(t1).^2));%计算q(t)
    q=q+1;

    u=zeros(1,2*L*T*fs);
    u(1:L*T*fs)=q(1:L*T*fs);
    for ii=1:L*T*fs
        u(ii+L*T*fs)=1-q(ii);
    end
    S0=sin(pi*h*u);%计算S0
    S1=zeros(1,2*L*T*fs);
    S1(1:(2*L-1)*T*fs)=S0(T*fs+1:2*L*T*fs);%S1是S0左移一个周期
    S2=zeros(1,2*L*T*fs);
    S2(1:(2*L-2)*T*fs)=S0(2*T*fs+1:2*L*T*fs);%S2是S0左移两个周期
    C0=S0(1:(2*L-2)*T*fs).*S1(1:(2*L-2)*T*fs).*S2(1:(2*L-2)*T*fs);  %幅度调制脉冲表示的组成部分

    gt=C0(1:4*fs);%原型滤波器

end