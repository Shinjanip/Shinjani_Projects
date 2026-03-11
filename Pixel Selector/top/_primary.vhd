library verilog;
use verilog.vl_types.all;
entity top is
    port(
        clk             : in     vl_logic;
        max_pixel       : out    vl_logic_vector(23 downto 0);
        min_pixel       : out    vl_logic_vector(23 downto 0)
    );
end top;
