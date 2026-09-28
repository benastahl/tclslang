// Valid version of the old test5: packed, unpacked and ascending ranges.
module top (
    input  wire              clk,
    input  wire  [7:0]       data_in,
    output reg   [3:0]       data_out,
    inout  wire              bidir_signal,
    input  wire  [3:0]       addr [0:15],
    output wire              flag,
    output wire  [1:0]       status [0:3],
    input  wire  [7:9]       i_select,
    input  logic [3:0][7:0]  packed_2d,
    output logic signed [15:0] sval
);
endmodule
