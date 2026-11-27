% *********************************************************************** 
% Function Name: cic3s_intp
%
% Description: CIC 3-stage interpolator
%
% Syntax: 
%      
%     cic_out = cic3s_intp(x) 
% 
%     cic_out : CIC 3-stage interpolator output
% 
%     cic_in  : CIC 3-stage interpolator input
%     R       : interpolation factor
%
% Last Update: 2018/07/10 by David Chen 
%  
% *********************************************************************** 
 
function cic_out= cic3s_intp(cic_in,R)

comb1_mem = 0;
comb2_mem = 0;
comb3_mem = 0;
intg1_out = 0;
intg2_out = 0;
intg3_out = 0;
cic_out   = [];

for ii = 1:length(cic_in)
    % comb section
    comb1_out = cic_in(ii) - comb1_mem;
    comb1_mem = cic_in(ii);
    
    comb2_out = comb1_out - comb2_mem;
    comb2_mem = comb1_out;    
    
    comb3_out = comb2_out - comb3_mem;
    comb3_mem = comb2_out;      

    % Upsampling by R
    comb3_out_us = [comb3_out;zeros(R-1,1)];

    for jj =0:R-1
        % integrator section
        intg1_out = intg1_out + comb3_out_us(jj+1);
        intg2_out = intg2_out + intg1_out;
        intg3_out = intg3_out + intg2_out;
        
        cic_out = [cic_out;intg3_out];
    end
end

