function  out =  Graymapping(bit1,bit2,bit3,bit4,M)

switch M

    case 4
    Graybit1 = bit3;
    Graybit2 = bit4;
    
    Binbit1  = Graybit1;
    Binbit2  = xor(Graybit2,Binbit1);
    
    out = Binbit1*2+Binbit2+1;
  
    case 16
    Graybit1 = bit1;
    Graybit2 = bit2;
    Graybit3 = bit3;
    Graybit4 = bit4;   
    
    Binbit1 = Graybit1;
    Binbit2 = xor(Graybit2,Binbit1);
    Binbit3 = xor(Graybit3,Binbit2);
    Binbit4 = xor(Graybit4,Binbit3);
    
    out =  Binbit1*2^3 + Binbit2*2^2 + Binbit3*2 + Binbit4 + 1;
end


end