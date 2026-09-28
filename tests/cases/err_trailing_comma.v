module top (
    input wire clk,               // Single wire input
    input wire [7:0] data_in,     // 8-bit input bus
    output reg [3:0] data_out,    // 4-bit output register
    inout wire bidir_signal,      // Bidirectional signal
    input wire [3:0] addr [0:15], // 16-entry address array
    output wire flag,             // Single wire output
    output wire [1:0] status [0:3], // 4-entry status array with 2 bits each
    input wire [7:9] i_select, hello_there_haha,
);


endmodule