interface pipeline_if #(parameter type T = logic [31:0]) ();
    logic valid;
    logic ready;
    T     data;

    modport in (
        input valid,
        input data,
        output ready
    );
    modport out (
        output valid,
        output data,
        input ready
    );
endinterface : pipeline_if
