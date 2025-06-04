module bar_t (
    input logic foo_port
);
endmodule

module foo (
    input logic foo_port
);

    wire my_signal;
    bar_t u_foo1 (.foo_port(my_signal));
    bar_t u_foo2 (.foo_port(my_signal));

endmodule

module top ();
    logic top_signal;
    foo u_top (.foo_port(top_signal));
    foo u_top2 (...)
endmodule
