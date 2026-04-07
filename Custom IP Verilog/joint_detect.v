module joint_detect #(
    parameter X_COORD = 1280,     // Frame Size
    parameter X_BIT_SIZE = 11,    // Number of Bits Required
    parameter Y_COORD = 720,
    parameter Y_BIT_SIZE = 10,
    parameter THRESHOLD_R = 25, // Maximum for 5 bits is 31
    parameter THRESHOLD_G = 45,   // Maximum for 6 bits is 63
    parameter THRESHOLD_B = 25,
    parameter MAX_CIRCLES = 13,    // Maximum number of circles
    parameter MIN_HEIGHT = 10,
    parameter MAX_GAP = 12,
    parameter MIN_WIDTH = 1
    
)(
    input clk,                 // Clock at which pixels will be sent on
    input aresetn,             // Synchronous active low reset
    input [15:0] pixel,        // RGB565 format is 16 bits
    input in_valid,            // Gating signal for incoming pixels
    output reg out_valid,      // High when a frame finishes and outputs are updated
    
    // BIT PACKED OUTPUT PORTS
    output [MAX_CIRCLES*X_BIT_SIZE-1:0] x_coords,
    output [MAX_CIRCLES*Y_BIT_SIZE-1:0] y_coords,
);
    // Internal coordinate registers to drive the packed outputs
    reg [X_BIT_SIZE-1:0] x_coord_array [MAX_CIRCLES-1:0];
    reg [Y_BIT_SIZE-1:0] y_coord_array [MAX_CIRCLES-1:0];

    // BIT PACKING ASSIGNMENTS
    genvar a;
    generate
        for (a = 0; a < MAX_CIRCLES; a = a + 1) begin : output_assignments
            assign x_coords[a*X_BIT_SIZE +: X_BIT_SIZE]  = x_coord_array[a];
            assign y_coords[a*Y_BIT_SIZE +: Y_BIT_SIZE]  = y_coord_array[a];
        end
    endgenerate

    // STAGE 1: Input valid/invalid and pixel detection (black or white)
    // Column count
    reg [X_BIT_SIZE-1:0] col_count;
    // Row count
    reg [Y_BIT_SIZE-1:0] row_count;
    // Detecting red, yellow, or green
    wire is_white = (pixel[15:11] >= THRESHOLD_R && pixel[10:5] >= THRESHOLD_G && pixel[4:0] >= THRESHOLD_B);
    wire is_red = (pixel[15:11] >= THRESHOLD_R && pixel[10:5] < THRESHOLD_G && pixel[4:0] < THRESHOLD_B);
    wire is_green = (pixel[15:11] < THRESHOLD_R && pixel[10:5] >= THRESHOLD_G && pixel[4:0] < THRESHOLD_B);
    //wire is_yellow = (pixel[15:11] >= THRESHOLD_R && pixel[10:5] >= THRESHOLD_G && pixel[4:0] < THRESHOLD_B);
    //wire is_purple = (pixel[15:11] >= THRESHOLD_R && pixel[10:5] < THRESHOLD_G && pixel[4:0] >= THRESHOLD_B);
    //wire is_blue = (pixel[15:11] < THRESHOLD_R && pixel[10:5] < THRESHOLD_G && pixel[4:0] >= THRESHOLD_B);
    
    // If the data in stage 1 is valid 
    reg pixel_valid;
    // Keep track of the column of pixel in stage 1
    reg [X_BIT_SIZE-1:0] pixel_col;
    // Keep track of the row of pixel in stage 1
    reg [Y_BIT_SIZE-1:0] pixel_row;
    // Stage 1 pixel b or w determination
    reg pixel_is_colour;
    reg stage_1_eof;

    // Input pixel receiving
    always @(posedge clk) begin
        if (!aresetn) begin
            col_count <= 0;
            row_count <= 0;
            pixel_valid <= 0;
            stage_1_eof <= 0;
        end 
        else begin
            // If the input pixel is valid
            if (in_valid) begin
                // Update information to be used for next stage
                pixel_valid <= 1'b1;
                pixel_col <= col_count;
                pixel_row <= row_count;
                pixel_is_colour <= is_red || is_green || is_white;
                
                // If this pixel is the last in the frame, pass a flag through
                stage_1_eof <= (col_count == X_COORD-1) && (row_count == Y_COORD-1);

                // Increment Counters
                if (col_count == X_COORD - 1) begin
                    // Reset column count
                    col_count <= 0;
                    // Reset row count
                    if (row_count == Y_COORD - 1) begin
                        row_count <= 0;
                    end 
                    else begin
                        row_count <= row_count + 1'b1;
                    end
                end 
                else 
                    col_count <= col_count + 1'b1;
            end 
            // If input is not valid
            else begin
                // Keep valid low
                pixel_valid <= 1'b0;
                stage_1_eof <= 1'b0;
            end
        end
    end

    // STAGE 2: Edge Detection using Pixel information (Updated with Black Pixel Denoising)
    reg in_segment;
    // Change the size based on our max gap
    reg [3:0] gap_count;
    reg [X_BIT_SIZE-1:0] cur_left_edge;

    // Edge detection information 
    reg [X_BIT_SIZE-1:0] pixel_left_edge;
    reg [X_BIT_SIZE-1:0] pixel_right_edge;
    // Bigger number addition
    (* use_dsp = "yes" *) reg [X_BIT_SIZE:0] pixel_row_mid; 
    // Passing in the row value to the next stage to keep track
    reg [Y_BIT_SIZE-1:0] pixel_row_2;
    reg pixel_falling_edge;
    reg stage_2_eof;

    // Edge Detection per Pixel
    always @(posedge clk) begin
        if (!aresetn) begin
            in_segment <= 0;
            gap_count <= 0;
            pixel_falling_edge <= 0;
            stage_2_eof <= 0;
        end else begin
            // Check if end of frame
            stage_2_eof <= stage_1_eof;

            if (pixel_valid) begin
                if (!in_segment) begin
                    // Detect Rising Edge: Start tracking a new potential joint using "in_segment"
                    if (pixel_is_colour) begin
                        in_segment <= 1'b1;
                        cur_left_edge <= pixel_col;
                        gap_count <= 0;
                    end
                    // No falling edge could be detected if we're not in a segment
                    pixel_falling_edge <= 1'b0;
                end 
                // If currently tracking a segment
                else begin
                    // We encounter a black pixel
                    if (!pixel_is_colour) begin
                        // Black pixel encountered: Check if gap is too large or we hit end of line
                        if (gap_count >= MAX_GAP || pixel_col == X_COORD - 1) begin
                            in_segment <= 1'b0; // Close segment
                            // Check MIN_WIDTH (Additional white pixel denoising --> unnecessary)
                            // Checks the total length of the white pixels excluding the black pixel noise
                            if ((pixel_col - gap_count - 1'b1) - cur_left_edge >= MIN_WIDTH) begin
                                // Consider as falling edge
                                pixel_falling_edge <= 1'b1;
                                pixel_left_edge <= cur_left_edge;
                                // Backtrack by gap count to true right edge
                                pixel_right_edge <= pixel_col - gap_count - 1'b1; 
                                pixel_row_mid <= (cur_left_edge + (pixel_col - gap_count - 1'b1)) >> 1;
                                pixel_row_2 <= pixel_row;
                            end 
                            // Discard the segment for being too narrow (filtering out white pixel noise)
                            else
                                pixel_falling_edge <= 1'b0; 
                            // Reset the gap count
                            gap_count <= 0;
                        end 
                        // The gap is not too large, so we continue to keep track of segment
                        else begin
                            gap_count <= gap_count + 1'b1;
                            pixel_falling_edge <= 1'b0;
                        end
                    end
                    // We encounter a white pixel
                    else begin
                        // Reset gap count
                        gap_count <= 0;
                        // Corner case: End of line while still on a white pixel (Still a falling edge)
                        if (pixel_col == X_COORD - 1) begin
                            in_segment <= 1'b0;
                            // Repeat white pixel denoising
                            if (pixel_col - cur_left_edge >= MIN_WIDTH) begin
                                pixel_falling_edge <= 1'b1;
                                pixel_left_edge <= cur_left_edge;
                                pixel_right_edge <= pixel_col;
                                pixel_row_mid <= (cur_left_edge + pixel_col) >> 1;
                                pixel_row_2 <= pixel_row;
                            end 
                            else
                                pixel_falling_edge <= 1'b0;
                        end 
                        else 
                            pixel_falling_edge <= 1'b0;
                    end
                end
            end 
            // The pixel is invalid --> Skip everything
            else 
                pixel_falling_edge <= 1'b0;
        end
    end

    // STAGE 3: Circle Tracking Arrays & Updates
    reg [X_BIT_SIZE-1:0] left_edges [MAX_CIRCLES-1:0];
    reg [X_BIT_SIZE-1:0] right_edges [MAX_CIRCLES-1:0];
    reg [Y_BIT_SIZE-1:0] first_rows [MAX_CIRCLES-1:0];
    reg [Y_BIT_SIZE-1:0] last_rows [MAX_CIRCLES-1:0];
    
    // Bigger number addition
    (* use_dsp = "yes" *) reg [Y_BIT_SIZE:0] y_mids [MAX_CIRCLES-1:0];
    (* use_dsp = "yes" *) reg [X_BIT_SIZE:0] x_mids [MAX_CIRCLES-1:0];
    
    reg [MAX_CIRCLES-1:0] circles;
    reg [3:0] circ_count;

    // Combinational Matching Logic
    reg match;
    reg [3:0] match_index;
    reg replace;
    reg [3:0] replace_index;
    integer i, j, k;

    // Combinational Block: Circle Tracking
    always @(*) begin
        match = 0;
        match_index = 0;
        replace = 0;
        replace_index = 0;

        // Check if part of a currently-tracked circle
        for (i = 0; i < MAX_CIRCLES; i = i + 1) begin
            // Have "matched" so that the if statement would not be run an unnecessary amount of times
            if (circles[i] && !match) begin
                if (((x_mids[i] >= pixel_left_edge && x_mids[i] <= pixel_right_edge) || (pixel_row_mid >= left_edges[i]) && (pixel_row_mid <= right_edges[i])) && (pixel_row_2 == last_rows[i] + 1'b1)) begin
                    // Part of this specific circle
                    match = 1;
                    match_index = i;
                end
            end
        end

        // Replace noisy white circles
        for (i = 0; i < MAX_CIRCLES; i = i + 1) begin
            // Don't need to check if there is a match
            // Basically break if we found a circle to replace
            if (circles[i] && !match && !replace) begin
                if (((last_rows[i] - first_rows[i]) < MIN_HEIGHT) && (pixel_row_2 != last_rows[i])) begin
                    replace = 1;
                    replace_index = i;
                end
            end
        end
    end

    // Updating Circle Trackers
    always @(posedge clk) begin
        if (!aresetn) begin
            out_valid <= 0;
            circ_count <= 0;
            circles <= 0;
            for (j = 0; j < MAX_CIRCLES; j = j + 1) begin
                x_coord_array[j] <= 0;
                y_coord_array[j] <= 0;
            end
        end 
        else begin
            // Keep out_valid low by default (or set it back after it was high)
            out_valid <= 1'b0;

            // Process a new detected segment (based on falling edge)
            if (pixel_falling_edge) begin
                if (match) begin
                    // Update the left and right edges of circle (for bounds of circle checking on next row)
                    left_edges[match_index] <= pixel_left_edge;
                    right_edges[match_index] <= pixel_right_edge;
                    // Update the last row
                    last_rows[match_index] <= pixel_row_2;
                    // Update the x-midpoints based on the row_mid and current circle x-mid
                    x_mids[match_index] <= (x_mids[match_index] + pixel_row_mid) >> 1;
                    // Update the y-midpoints based on the current row
                    y_mids[match_index] <= (first_rows[match_index] + pixel_row_2) >> 1;
                end 
                else if (replace) begin
                    // Update with new left/right edges, treat as new circle
                    left_edges[replace_index] <= pixel_left_edge;
                    right_edges[replace_index] <= pixel_right_edge;
                    x_mids[replace_index] <= pixel_row_mid;
                    first_rows[replace_index] <= pixel_row_2;
                    last_rows[replace_index] <= pixel_row_2;
                    y_mids[replace_index] <= pixel_row_2;
                end 
                // A new circle, but only do this if there are less valid circles than the total number we expect
                else if (circ_count < MAX_CIRCLES) begin
                    // Increment circle count, add into circle tracker
                    circles[circ_count] <= 1'b1;
                    left_edges[circ_count] <= pixel_left_edge;
                    right_edges[circ_count] <= pixel_right_edge;
                    x_mids[circ_count] <= pixel_row_mid;
                    first_rows[circ_count] <= pixel_row_2;
                    last_rows[circ_count] <= pixel_row_2;
                    y_mids[circ_count] <= pixel_row_2;
                    circ_count <= circ_count + 1'b1;
                end
            end

            // If end of frame (propagated from previous stages)
            if (stage_2_eof) begin
                out_valid <= 1'b1;
                // Set circle count back to zero
                circ_count <= 0;
                
                for (k = 0; k < MAX_CIRCLES; k = k + 1) begin
                    if (circles[k]) begin
                        x_coord_array[k] <= x_mids[k];
                        y_coord_array[k] <= y_mids[k];
                    end 
                    // Ignore circles that should not be there
                    else begin
                        x_coord_array[k] <= 0;
                        y_coord_array[k] <= 0;
                    end
                    
                    // Reset working tracking arrays
                    circles[k] <= 0;
                    x_mids[k] <= 0;
                    y_mids[k] <= 0;
                    first_rows[k] <= 0;
                    last_rows[k] <= 0;
                    left_edges[k] <= 0;
                    right_edges[k] <= 0;
                end
            end
        end
    end

endmodule