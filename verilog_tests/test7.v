// Define an interface (for interface connection example)
interface simple_ifc;
    logic sig;
endinterface

// Submodule to connect things into
module sub(input logic a);
endmodule

// Top module
module top(
    input logic in_var,         // <-- Port (logic → Variable)
    input wire in_net,           // <-- Port (wire → Net)
    output logic out_var         // <-- Port (logic → Variable)
);

    parameter int WIDTH = 1;     // <-- Parameter

    wire wire_signal;            // <-- Net (wire)
    logic logic_signal;          // <-- Variable (logic)
    simple_ifc ifc_inst();       // <-- Interface instance

    // Different types of port connections:

    sub u1 (.a(in_var));          // Driver: Variable (PortSignal in_var)
    sub u2 (.a(in_net));          // Driver: Net (PortSignal in_net)
    sub u3 (.a(wire_signal));     // Driver: Net
    sub u4 (.a(logic_signal));    // Driver: Variable
    sub u5 (.a(1'b1));                 // Driver: Constant literal
    sub u6 (.a(WIDTH));           // Driver: Parameter
    sub u7 (.a(ifc_inst.sig));    // Driver: Interface signal (sig from simple_ifc)

endmodule
