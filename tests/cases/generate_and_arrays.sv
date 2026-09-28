// Instances that live inside generate blocks and instance arrays.
module cell_a(input logic d);
endmodule

module top #(parameter int N = 2, parameter bit USE_B = 1) (input logic [3:0] d);
    cell_a u_array [1:0] (.d(d[1:0]));

    for (genvar i = 0; i < N; i++) begin : g_loop
        cell_a u_gen (.d(d[i]));
    end

    if (USE_B) begin : g_if
        cell_a u_cond (.d(d[3]));
    end else begin : g_else
        cell_a u_never (.d(d[2]));
    end
endmodule
