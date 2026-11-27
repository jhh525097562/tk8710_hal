% *********************************************************************** 
% Function Name: cic3s_decm
%
% Description: CIC 3-stage decimator
%
% Syntax: 
%      
%     cic_out = cic3s_decm(x) 
% 
%     cic_out : CIC 3-stage decimator output
% 
%     cic_in  : CIC 3-stage decimator input
%     R       : decimation factor
%
% Last Update: 2018/07/10 by David Chen 
%  
% *********************************************************************** 
 
function cic_out= cic3s_decm(cic_in,R)

intg1_out = 0;
intg2_out = 0;
intg3_out = 0;
comb1_mem = 0;
comb2_mem = 0;
comb3_mem = 0;
cic_out   = [];
jj = R-1;


for ii = 1:length(cic_in)
	% integrator section
	intg1_out = intg1_out + cic_in(ii);
	intg2_out = intg2_out + intg1_out;
	intg3_out = intg3_out + intg2_out;    
    
    if (jj==0)
        jj = R-1;
    else
        jj = jj-1;
    end
        
    % downsampling by R
    if (jj==R-1)
        % comb section
        comb1_out = intg3_out - comb1_mem;
        comb1_mem = intg3_out;
    
        comb2_out = comb1_out - comb2_mem;
        comb2_mem = comb1_out;    
    
        comb3_out = comb2_out - comb3_mem;
        comb3_mem = comb2_out;
        
        cic_out = [cic_out;comb3_out];
    end

end

