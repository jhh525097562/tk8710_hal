function  out = PhaseMod4Psk(in)

if in == [0;0]
        out = 0;
elseif  in == [0;1]
        out = 1.5708;
elseif  in == [1;0]
        out = - 1.5708;
elseif  in == [1;1]
        out = 3.1416;
end

end