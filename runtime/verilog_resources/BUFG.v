
// This module is not actually meant to provide the functionality of BUFG,
// but rather just provide a workaround so that the simulator doesn't have
// to deal with it. This file is not provided to Vivado.

module BUFG(I, O);
    input I;
    output O;
    assign O = I;
endmodule;