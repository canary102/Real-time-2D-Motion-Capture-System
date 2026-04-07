`timescale 1ns/1ps

module sim_id();
    // Simulation Parameters (Frame Size)
    localparam X_SIZE = 1280;
    localparam X_BIT_SIZE = 11;
    localparam Y_SIZE = 720;
    localparam Y_BIT_SIZE = 10;
    // Old thresholds for specific image
    localparam THRESHOLD_R = 25;
    localparam THRESHOLD_G = 32;
    localparam THRESHOLD_B = 25;
    // Test Image Parameters
    localparam MAX_CIRCLES = 6;
    localparam MIN_HEIGHT = 5;
    localparam MAX_GAP = 10;
    localparam MIN_WIDTH = 3;
    localparam C_S00_AXIS_TDATA_WIDTH = 24;
    localparam C_S00_AXI_DATA_WIDTH = 32;
    localparam C_S00_AXI_ADDR_WIDTH = 6;
    localparam TOTAL_PIXELS = X_SIZE*Y_SIZE;

    // Image Memory 
    reg [23:0] image_rom [0:TOTAL_PIXELS-1];
    initial begin
        $display("Loading image data...");
        $readmemh("dirty_image_data.hex", image_rom);
    end
    
    // Inputs to DUT
    reg clk;
    reg aresetn;

    // AXI Stream
    wire t_ready;
    reg [C_S00_AXIS_TDATA_WIDTH-1 : 0] t_data;
    reg t_valid;
    wire [(C_S00_AXIS_TDATA_WIDTH/8)-1 : 0] t_strb;
    wire t_last;

    // AXI Lite signals (Unused)
    reg [C_S00_AXI_ADDR_WIDTH-1 : 0] awaddr;
    wire [2 : 0] awprot;
    reg awvalid;
    wire awready;
    wire [C_S00_AXI_DATA_WIDTH-1 : 0] wdata;
    wire [(C_S00_AXI_DATA_WIDTH/8)-1 : 0] wstrb;
    wire wvalid;
    wire wready;
    wire [1 : 0] bresp;
    wire bvalid;
    wire bready;
    wire [C_S00_AXI_ADDR_WIDTH-1 : 0] araddr;
    wire [2 : 0] arprot;
    wire arvalid;
    wire arready;
    wire [C_S00_AXI_DATA_WIDTH-1 : 0] rdata;
    wire [1 : 0] rresp;
    wire rvalid;
    wire rready;

    // Instantiate the dut
    custom_ip #(
        .X_SIZE(X_SIZE),
        .X_BIT_SIZE(X_BIT_SIZE),
        .Y_SIZE(Y_SIZE),
        .Y_BIT_SIZE(Y_BIT_SIZE),
        .THRESHOLD_R(THRESHOLD_R),
        .THRESHOLD_G(THRESHOLD_G),
        .THRESHOLD_B(THRESHOLD_B),
        .MAX_CIRCLES(MAX_CIRCLES),
        .MIN_HEIGHT(MIN_HEIGHT),
        .MAX_GAP(MAX_GAP),
        .MIN_WIDTH(MIN_WIDTH),
        .C_S00_AXIS_TDATA_WIDTH(C_S00_AXIS_TDATA_WIDTH),
        .C_S00_AXI_DATA_WIDTH(C_S00_AXI_DATA_WIDTH),
        .C_S00_AXI_ADDR_WIDTH(C_S00_AXI_ADDR_WIDTH)
    ) dut (
        .s00_axis_aclk(clk),
        .s00_axis_aresetn(aresetn),
        .s00_axis_tready(t_ready),
        .s00_axis_tdata(t_data),
        .s00_axis_tvalid(t_valid),
        .s00_axis_tstrb(t_strb),
        .s00_axis_tlast(t_last),
        .s00_axi_aclk(clk),
        .s00_axi_aresetn(aresetn),
        .s00_axi_awaddr(awaddr),
        .s00_axi_awprot(awprot),
        .s00_axi_awvalid(awvalid),
        .s00_axi_awready(awready),
        .s00_axi_wdata(wdata),
        .s00_axi_wstrb(wstrb),
        .s00_axi_wvalid(wvalid),
        .s00_axi_wready(wready),
        .s00_axi_bresp(bresp),
        .s00_axi_bvalid(bvalid),
        .s00_axi_bready(bready),
        .s00_axi_araddr(araddr),
        .s00_axi_arprot(arprot),
        .s00_axi_arvalid(arvalid),
        .s00_axi_arready(arready),
        .s00_axi_rdata(rdata),
        .s00_axi_rresp(rresp),
        .s00_axi_rvalid(rvalid),
        .s00_axi_rready(rready)
    );

    // Clock generation (100MHz)
    always #5 clk = ~clk;

    initial begin
        clk = 0;
        t_data = 0;
        t_valid = 0;
        aresetn = 0;
        #30;
        aresetn = 1;
        #10;
        // Enable t_ready by writing to slave register
        awaddr = 6'b001111;
        awvalid = 1;
        // Wait one cycle for t_ready to go high
        @(posedge clk);
        
        
        for (integer r = 0; r < Y_SIZE; r = r + 1) begin
            for (integer c = 0; c < X_SIZE; c = c + 1) begin
                
                // TEST CASE 1
                // 1. Fetch from memory instead of IF/ELSE chain
                t_data = image_rom[r * X_SIZE + c];
                
                // 2. Handshake Logic
                t_valid = 1;
                
                // Wait for the DUT to be ready (Standard AXI Stream behavior)
                // If your DUT doesn't always have t_ready high, we wait here:
                while (!t_ready) @(posedge clk);
                @(posedge clk);


                // 3. --- REPEATING STALL LOGIC ---
                // Every 100 pixels, de-assert valid for 5 cycles AFTER the pixel is sent
                if (((c + r * X_SIZE) % 100 == 0) && (c + r * X_SIZE) != 0) begin
                    t_valid = 0;
                    repeat(5) @(posedge clk);
                end
            end
        end
        
        #9216000;
        $stop;
    end

endmodule