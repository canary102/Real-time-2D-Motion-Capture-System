
`timescale 1 ns / 1 ps

module custom_ip #
(
    // Users to add parameters here
    parameter X_SIZE = 1280,     // Frame Size
    parameter X_BIT_SIZE = 11,    // Number of Bits Required
    parameter Y_SIZE = 720,
    parameter Y_BIT_SIZE = 10,
    parameter THRESHOLD_R_B = 25, // Maximum for 5 bits is 31
    parameter THRESHOLD_G = 50,   // Maximum for 6 bits is 63
    parameter MAX_CIRCLES = 13,    // Maximum number of circles
    parameter MIN_HEIGHT = 10,
    parameter MAX_GAP = 12,
    parameter MIN_WIDTH = 1,
    parameter TOTAL_THRESHOLD = 90,
    // User parameters ends
    // Do not modify the parameters beyond this line


    // Parameters of Axi Slave Bus Interface S00_AXIS
    // 24-bit for rgb888 pizel format input
    parameter integer C_S00_AXIS_TDATA_WIDTH	= 32,

    // Parameters of Axi Slave Bus Interface S00_AXI
    parameter integer C_S00_AXI_DATA_WIDTH	= 32,
    parameter integer C_S00_AXI_ADDR_WIDTH	= 6
)
(
    // Users to add ports here

    // User ports ends
    // Do not modify the ports beyond this line


    // Ports of Axi Slave Bus Interface S00_AXIS
    input  s00_axis_aclk,
    input  s00_axis_aresetn,
    output  s00_axis_tready,
    input [C_S00_AXIS_TDATA_WIDTH-1 : 0] s00_axis_tdata,
    input  s00_axis_tvalid,
    // UNUSED
    input [(C_S00_AXIS_TDATA_WIDTH/8)-1 : 0] s00_axis_tstrb,
    input  s00_axis_tlast,

    // Ports of Axi Slave Bus Interface S00_AXI
    input  s00_axi_aclk,
    input  s00_axi_aresetn,
    input [C_S00_AXI_ADDR_WIDTH-1 : 0] s00_axi_awaddr,
    input [2 : 0] s00_axi_awprot,
    input  s00_axi_awvalid,
    output  s00_axi_awready,
    input [C_S00_AXI_DATA_WIDTH-1 : 0] s00_axi_wdata,
    input [(C_S00_AXI_DATA_WIDTH/8)-1 : 0] s00_axi_wstrb,
    input  s00_axi_wvalid,
    output  s00_axi_wready,
    output [1 : 0] s00_axi_bresp,
    output  s00_axi_bvalid,
    input  s00_axi_bready,
    input [C_S00_AXI_ADDR_WIDTH-1 : 0] s00_axi_araddr,
    input [2 : 0] s00_axi_arprot,
    input  s00_axi_arvalid,
    output  s00_axi_arready,
    output [C_S00_AXI_DATA_WIDTH-1 : 0] s00_axi_rdata,
    output [1 : 0] s00_axi_rresp,
    output  s00_axi_rvalid,
    input  s00_axi_rready,
    //Debug ports
    output cbit,
    output [X_BIT_SIZE-1:0] x_loc,
    output [Y_BIT_SIZE-1:0] y_loc,
    output [3:0] circ_coun,
    output [15:0] pdata,
    output mtch,
    output rplc,
    output [3:0] mtch_idx,
    output [3:0] rplc_idx,
    output r_high,
    output g_high,
    output b_high,
    output [Y_BIT_SIZE-1:0] f_row2,
    output [Y_BIT_SIZE-1:0] f_row5,
    output [Y_BIT_SIZE-1:0] l_row2,
    output [Y_BIT_SIZE-1:0] l_row5,
    //Also add all AXI stream ports as an additional one (valid, ready, data)
    output [C_S00_AXIS_TDATA_WIDTH-1:0] tdata,
    output tvalid,
    output cready,
    output o_valid,
    output i_valid
    //Also add out_valid, in_valid
);

    assign tvalid = s00_axis_tvalid;
    assign tdata = s00_axis_tdata;
    assign cready = tready;
    assign o_valid = out_valid;
    assign i_valid = in_valid;
    // Wires
    // out_valid is assigned to the last slave register (slv_reg15)
    wire out_valid;
    wire [MAX_CIRCLES*X_BIT_SIZE-1:0] x_coords;
    wire [MAX_CIRCLES*Y_BIT_SIZE-1:0] y_coords;
    
    // Data input fed into joint_detect (must be converted to RGB565)
    wire [15:0] data_in;
    // Convert from RBG888 by bit slicing (same as shifting each part) to RGB565
    assign data_in = {s00_axis_tdata[23:19], s00_axis_tdata[7:2], s00_axis_tdata[15:11]};
    // Keep t_ready high at all times, but if we write to the specified addresses it can start/stop the frame processing
    reg tready;
    assign s00_axis_tready = tready;
    
    always@(posedge s00_axi_aclk) begin
        if (s00_axi_awaddr==6'b001111 && s00_axi_awvalid) begin
           tready <= 1'b1;
        end
        if (s00_axi_awaddr==6'b000001 && s00_axi_awvalid) begin
            tready <= 1'b0;
        end
        else if (out_valid || !s00_axi_aresetn) begin
            tready <= 1'b0;
        end
    end
    // in_valid should be high if both the ready and valid signals of stream slave are high
    wire in_valid;
    // Assign in_valid to be when both t_ready and t_valid are high
    assign in_valid = s00_axis_tready && s00_axis_tvalid;
    
    // Instantiation of Axi Bus Interface S00_AXI
    myip_v1_0_S00_AXI # ( 
        .X_BIT_SIZE(X_BIT_SIZE),
        .Y_BIT_SIZE(Y_BIT_SIZE),
        .MAX_CIRCLES(MAX_CIRCLES),
        .C_S_AXI_DATA_WIDTH(C_S00_AXI_DATA_WIDTH),
        .C_S_AXI_ADDR_WIDTH(C_S00_AXI_ADDR_WIDTH)
    ) myip_v1_0_S00_AXI_inst (
        .x_coords(x_coords),
        .y_coords(y_coords),
        .out_valid(out_valid),
        .S_AXI_ACLK(s00_axi_aclk),
        .S_AXI_ARESETN(s00_axi_aresetn),
        .S_AXI_AWADDR(s00_axi_awaddr),
        .S_AXI_AWPROT(s00_axi_awprot),
        .S_AXI_AWVALID(s00_axi_awvalid),
        .S_AXI_AWREADY(s00_axi_awready),
        .S_AXI_WDATA(s00_axi_wdata),
        .S_AXI_WSTRB(s00_axi_wstrb),
        .S_AXI_WVALID(s00_axi_wvalid),
        .S_AXI_WREADY(s00_axi_wready),
        .S_AXI_BRESP(s00_axi_bresp),
        .S_AXI_BVALID(s00_axi_bvalid),
        .S_AXI_BREADY(s00_axi_bready),
        .S_AXI_ARADDR(s00_axi_araddr),
        .S_AXI_ARPROT(s00_axi_arprot),
        .S_AXI_ARVALID(s00_axi_arvalid),
        .S_AXI_ARREADY(s00_axi_arready),
        .S_AXI_RDATA(s00_axi_rdata),
        .S_AXI_RRESP(s00_axi_rresp),
        .S_AXI_RVALID(s00_axi_rvalid),
        .S_AXI_RREADY(s00_axi_rready)
    );
    
    // Add user logic here
    // Instantiation of joint_detect
    /*
    1. Clock --> AXI stream clock
    2. Reset --> Synchronous active-low reset configured like it is in AXI stream
    3. Pixel data comes in on tdata
    4. input is valid if both t_valid and t_ready are high (handshake)
    5. out_valid, x and y coords used as input into AXI slave to be read by Microblaze
    */
    joint_detect # (
        .X_COORD(X_SIZE),
        .X_BIT_SIZE(X_BIT_SIZE),
        .Y_COORD(Y_SIZE),
        .Y_BIT_SIZE(Y_BIT_SIZE),
        .MAX_CIRCLES(MAX_CIRCLES),
        .MIN_HEIGHT(MIN_HEIGHT),
        .MAX_GAP(MAX_GAP),
        .MIN_WIDTH(MIN_WIDTH),
        .TOTAL_THRESHOLD(TOTAL_THRESHOLD)
    ) joint_detection (
        .clk(s00_axis_aclk),
        .aresetn(s00_axis_aresetn),
        .pixel(data_in),
        .in_valid(in_valid),
        .out_valid(out_valid),
        .x_coords(x_coords),
        .y_coords(y_coords),
        .cbit(cbit),
        .x_loc(x_loc),
        .y_loc(y_loc),
        .circ_coun(circ_coun),
        .pdata(pdata),
        .rplc(rplc),
        .mtch(mtch),
        .rplc_idx(rplc_idx),
        .mtch_idx(mtch_idx),
        .r_high(r_high),
        .b_high(b_high),
        .g_high(g_high)
);

// User logic ends

endmodule