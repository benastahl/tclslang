// Valid version of the old test4: the same module instantiated twice, each
// with its own sub-instances. slang may share one body between identical
// instances; cell names must still be per-instance.
module bar_t (
    input logic foo_port
);
endmodule

module foo (
    input logic foo_port
);
    wire my_signal;
    bar_t u_foo1 (.foo_port(my_signal));
    bar_t u_foo2 (.foo_port(foo_port));
endmodule

module top ();
    logic top_signal;
    foo u_top (.foo_port(top_signal));
    foo u_top2 (.foo_port(1'b0));
endmodule
