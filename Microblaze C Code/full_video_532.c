/************************************************************************/
/*																		*/
/*	video_demo.c	--	Real-time 2D Motion Capture System				*/
/*																		*/
/************************************************************************/

/* ------------------------------------------------------------ */
/*				Include File Definitions						*/
/* ------------------------------------------------------------ */

#include "video_demo.h"
#include "video_capture/video_capture.h"
#include "display_ctrl/display_ctrl.h"
#include "intc/intc.h"
#include <stdio.h>
#include "xuartlite_l.h"
#include "math.h"
#include <ctype.h>
#include <stdlib.h>
#include "xil_types.h"
#include "xil_cache.h"
#include "xparameters.h"
#include "sleep.h"
#include <string.h>
#include "xtmrctr.h"

/*
 * XPAR redefines
 */
#define DYNCLK_BASEADDR XPAR_AXI_DYNCLK_0_S_AXI_LITE_BASEADDR
#define VGA_VDMA_ID XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID XPAR_VTC_0_DEVICE_ID
#define VID_VTC_ID XPAR_VTC_1_DEVICE_ID
#define VID_GPIO_ID XPAR_AXI_GPIO_VIDEO_DEVICE_ID
#define VID_VTC_IRPT_ID XPAR_INTC_0_VTC_1_VEC_ID
#define VID_GPIO_IRPT_ID XPAR_INTC_0_GPIO_0_VEC_ID
#define SCU_TIMER_ID XPAR_AXI_TIMER_0_DEVICE_ID
#define UART_BASEADDR XPAR_UARTLITE_0_BASEADDR

#define XPAR_INTC_0_MARKER_DETECTER_V4_0_O_VALID_VEC_ID XPAR_MICROBLAZE_0_AXI_INTC_MARKER_DETECTER_V4_0_O_VALID_INTR
#define D_PAD_GPIO_ID XPAR_GPIO_1_DEVICE_ID
#define VID_S2MM_IRPT_ID XPAR_INTC_0_AXIVDMA_0_S2MM_INTROUT_VEC_ID
#define DISP_MM2S_IRPT_ID XPAR_INTC_0_AXIVDMA_0_MM2S_INTROUT_VEC_ID

/* ------------------------------------------------------------ */
/*				Global Variables								*/
/* ------------------------------------------------------------ */

DisplayCtrl dispCtrl;
XAxiVdma vdma;
VideoCapture videoCapt;
INTC intc;
char fRefresh;

u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME] __attribute__((aligned(128)));
u8 *pFrames[DISPLAY_NUM_FRAMES];

const ivt_t ivt[] = {
	videoGpioIvt(VID_GPIO_IRPT_ID, &videoCapt),
	videoVtcIvt(VID_VTC_IRPT_ID, &(videoCapt.vtc))
};

// Structure to hold joint coordinates
typedef struct {
    u32 x;
    u32 y;
} Joint;

// Custom IP 
#define IP_VDMA_DEVICE_ID XPAR_AXIVDMA_1_DEVICE_ID
#define CUSTOMIP_BASE_ADDR XPAR_MARKER_DETECTER_V4_0_BASEADDR

XAxiVdma ip_vdma;

// GUI & System Globals
XGpio DpadGpio;
XTmrCtr TimerInst;

// GUI Modes 
int current_mode = 0;
int video_paused = 0;
int photo_captured = 0;
int button_debounced = 1;

// Model Tracking
int current_model = 0;
int last_drawn_model = 0;
#define MAX_MODELS 2

volatile u32 frame_count = 0;
u32 last_timer_val = 0;
int current_fps = 0;
char fps_str[10] = "00 FPS";

// V-SYNC GLOBALS
volatile int vdma_frame_done = 0;

void VdmaS2MmISR(void *CallbackRef) {
    XAxiVdma_IntrClear(&vdma, XAXIVDMA_IXR_COMPLETION_MASK, XAXIVDMA_WRITE);
    vdma_frame_done = 1;
}

int init_ip_vdma(u32 frame_address, u32 width, u32 height, u32 stride)
{
	ip_vdma.IsReady = 0;
    XAxiVdma_DmaSetup setup;
    XAxiVdma_Config *cfg;

    cfg = XAxiVdma_LookupConfig(IP_VDMA_DEVICE_ID);
    if (!cfg) return XST_FAILURE;

    u32 base = cfg->BaseAddress;
    Xil_Out32(base + 0x00, Xil_In32(base + 0x00) | (1<<1));
    usleep(1000);

    int Status = XAxiVdma_CfgInitialize(&ip_vdma, cfg, cfg->BaseAddress);
    if (Status != XST_SUCCESS) return XST_FAILURE;

    memset(&setup, 0, sizeof(setup));
    setup.VertSizeInput = height;
    setup.HoriSizeInput = (width)* 3-1;
    setup.Stride = stride;
    setup.FrameDelay = 0;
    setup.EnableCircularBuf = 1;
    setup.EnableSync = 0;
    setup.PointNum = 0;
    setup.EnableFrameCounter = 0;
    setup.FixedFrameStoreAddr = 0;
    setup.FrameStoreStartAddr[0] = pFrames[0];
	setup.FrameStoreStartAddr[1] = pFrames[1];
	setup.FrameStoreStartAddr[2] = pFrames[2];

    if (XAxiVdma_DmaConfig(&ip_vdma, XAXIVDMA_READ, &setup) != XST_SUCCESS) return XST_FAILURE;
    if (XAxiVdma_DmaSetBufferAddr(&ip_vdma, XAXIVDMA_READ, setup.FrameStoreStartAddr) != XST_SUCCESS) return XST_FAILURE;
    if (XAxiVdma_DmaStart(&ip_vdma, XAXIVDMA_READ) != XST_SUCCESS) return XST_FAILURE;
    if (XAxiVdma_StartParking(&ip_vdma, 0, XAXIVDMA_READ) != XST_SUCCESS) return XST_FAILURE;

    return XST_SUCCESS;
}

// Read Registers of the custom IP 
void read_ip_registers(Joint *detected_joints)
{
    for (int i = 0; i < 16; i++) {
        u32 val = Xil_In32(CUSTOMIP_BASE_ADDR + (i*4));
        u32 y =  val        & 0x3FF;
        u32 x = (val >> 10) & 0x7FF;
        // xil_printf("Reg %2d: X=%4d | Y=%4d\r\n", i, x, y); // Removed debug print to reduce serial lag

        if (i < 13) {
        	detected_joints[i].y = y;
        	detected_joints[i].x = x;
        }
    }
}

/*
Skeleton visualization
         0
         |
       1 2 3
      /  |  \
     4   |   5
    |    |    |
    6    |    7
         8
        / \
       /   \
      9     10
      |     |
     11    12
Connections
0->2
2->1,3,8
1->4->6
3->5->7
8->9,10
9->11
10->12
/*
Hardcoded Skeleton Drawing Algorithm
Sorts all markers by Y coordinate
Then from top of the screen to the bottom (Y coord) 0 > 1,2,3 > 4,5 > 6,7 > 8 > 9,10 > 11,12
Then from left of the screen to the right (X coord) 1 > 2 > 3 / 4 > 5 / 6 > 7 / 9 > 10 / 11 > 12
*/

// Swap joints in structure 
void swap_joint(Joint *a, Joint *b) {
    Joint temp = *a;
    *a = *b;
    *b = temp;
}

// Sort joints by x coordinate 
void sort_by_x(Joint *arr, int start, int end) {
    for (int i = start; i <= end; i++) {
        for (int j = i + 1; j <= end; j++) {
            if (arr[i].x > arr[j].x) swap_joint(&arr[i], &arr[j]);
        }
    }
}

// Hardcoded Joint Assignment 
void JointAssignment(Joint *raw_joints, Joint *mapped_joints) {
    for (int i = 0; i < 13; i++) mapped_joints[i] = raw_joints[i];
    for (int i = 0; i < 13; i++) {
        for (int j = i + 1; j < 13; j++) {
            if (mapped_joints[i].y > mapped_joints[j].y) swap_joint(&mapped_joints[i], &mapped_joints[j]);
        }
    }
    sort_by_x(mapped_joints, 1, 3);
    sort_by_x(mapped_joints, 4, 5);
    sort_by_x(mapped_joints, 6, 8);
    sort_by_x(mapped_joints, 9, 10);
    sort_by_x(mapped_joints, 11, 12);
}

// Set pixel in framebuffer
// Note: uses RBG format 
void set_pixel(u8 *frame, u32 stride, int x, int y, u8 r, u8 g, u8 b) {
    if (x >= 0 && x < 1280 && y >= 0 && y < 720) {
        u32 iPixelAddr = (y * stride) + (x * 3);
        frame[iPixelAddr]     = g; // 1st Byte is Green
        frame[iPixelAddr + 1] = b; // 2nd Byte is Blue
        frame[iPixelAddr + 2] = r; // 3rd Byte is Red
    }
}

/*
Line drawing algorithm using Bresenham's Line Algorithm
Optimized with pointer addition 
*/
void draw_line_optimized(u8 *frame, u32 stride, int x0, int y0, int x1, int y1, u8 r, u8 g, u8 b) {
    int dx = abs_int(x1 - x0);
    int dy = -abs_int(y1 - y0);
    int err = dx + dy;
    int e2;
    int sx;
    if (x0 < x1) {
        sx = 3;
    } else {
        sx = -3;
    }
    int sy;
    if (y0 < y1) {
        sy = (int)stride;
    } else {
        sy = -(int)stride;
    }

    // Bounds checking
    if (x0 < 0 || x0 >= 1280 || y0 < 0 || y0 >= 720 || x1 < 0 || x1 >= 1280 || y1 < 0 || y1 >= 720) {
        return;
    }

    u32 addr = (y0 * stride) + (x0 * 3);
    while (1) {
        frame[addr] = g;
        frame[addr + 1] = b;
        frame[addr + 2] = r;

        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            if (sx > 0) {
                x0 += 1;
            } else {
                x0 -= 1;
            }
            addr += sx;
        }
        if (e2 <= dx) {
            err += dx;
            if (sy > 0) {
                y0 += 1;
            } else {
                y0 -= 1;
            }
            addr += sy;
        }
    }
}

// 5*5 joint marker 
void draw_joint_marker(u8 *frame, u32 stride, int x, int y, u8 r, u8 g, u8 b) {
    for (int i = -2; i <= 2; i++) {
        for (int j = -2; j <= 2; j++) {
            set_pixel(frame, stride, x + i, y + j, r, g, b);
        }
    }
}

// Skeleton drawing function, directly connects joints together based on hardcoded assignments 
void DrawSkeleton(u8 *frame, u32 stride, Joint *raw_joints, float stretch_factor, u8 joint_r, u8 joint_g, u8 joint_b, u8 line_r,  u8 line_g,  u8 line_b) {
    Joint mapped_joints[13];
    JointAssignment(raw_joints, mapped_joints);

    for (int i = 0; i < 13; i++) {
        if (mapped_joints[i].x != 0 || mapped_joints[i].y != 0) {
            int stretched_x = (int)((float)mapped_joints[i].x * stretch_factor);
            draw_joint_marker(frame, stride, stretched_x, mapped_joints[i].y, joint_r, joint_g, joint_b);
        }
    }

    int edges[12][2] = {
        {0,2}, {1,2}, {3,2}, {1,4}, {4,6}, {3,5},
        {5,8}, {2,7}, {7,9}, {9,11}, {7,10}, {10,12}
    };

    for (int i = 0; i < 12; i++) {
        int pt1 = edges[i][0];
        int pt2 = edges[i][1];

        if ((mapped_joints[pt1].x != 0 || mapped_joints[pt1].y != 0) && (mapped_joints[pt2].x != 0 || mapped_joints[pt2].y != 0)) {
            int x0_stretched = (int)((float)mapped_joints[pt1].x * stretch_factor);
            int x1_stretched = (int)((float)mapped_joints[pt2].x * stretch_factor);
            draw_line_optimized(frame, stride, x0_stretched, mapped_joints[pt1].y, x1_stretched, mapped_joints[pt2].y, line_r, line_g, line_b);
        }
    }
}

// Skeleton Color Detection 
/*
Skeleton visualization (Color)
         0  
         |
       1 2 3
      /  |  \
     4   |   5
    |    |    |
    6    |    7
         8
        / \
       /   \
      9     10
      |     |
     11    12
3 Colors - W, G, R 
         W  
         |
       W W W
      /  |  \
     G   |   G
    |    |    |
    R    |    R
         W
        / \
       /   \
      W     W
      |     |
      G     G
Connections
0->2
2->1,3,8
1->4->6
3->5->7
8->9,10
9->11
10->12

0, 1, 2, 3, 8, 9, 10 - White 
-> Sort by y coordinate into four baskets (0), (1, 2, 3), (8), (9, 10)
-> Sort baskets by x coordinates to get final mapping 
4, 5, 11, 12 - Green
-> Sort by y coordinate into two baskets, then sort baskets by x coordinates
6, 7 - Red 
-> Sort by x coordinate

POSITION ASSUMPTIONS
For a given color: 
- The top row must never go below the bottom row, i.e. for W marker 0 must never go below markers 1, 2, or 3
- For a row, the left must never go past the right i.e. for W markers 1, 2, and 3, marker 1 must always remain to the left of 2 and 3 
*/
extern int joint_colours[13];

void AssignJointsByColor(Joint *raw_joints, int *colors, Joint *mapped_joints) {
    for(int i = 0; i < 13; i++) {
        mapped_joints[i].x = 0;
        mapped_joints[i].y = 0;
    }

    Joint reds[13], greens[13], whites[13];
    int r_count = 0, g_count = 0, w_count = 0;

    for(int i = 0; i < 13; i++) {
        if(raw_joints[i].x == 0 && raw_joints[i].y == 0) continue;

        switch(colors[i]) {
            case 2: reds[r_count++]   = raw_joints[i]; break; // Red
            case 3: greens[g_count++] = raw_joints[i]; break; // Green
            case 1: whites[w_count++] = raw_joints[i]; break; // White
        }
    }

    // White marker sorting (7 markers) - Head, Shoulders, Neck/Chest, Pelvis, Knees
    for (int i = 0; i < w_count; i++) {
        for (int j = i + 1; j < w_count; j++) {
            if (whites[i].y > whites[j].y) swap_joint(&whites[i], &whites[j]);
        }
    }
    if (w_count > 0) mapped_joints[0] = whites[0]; // Head 
    if (w_count >= 4) {
        sort_by_x(whites, 1, 3);
        mapped_joints[1] = whites[1]; // L_Shoulder
        mapped_joints[2] = whites[2]; // Neck/Chest
        mapped_joints[3] = whites[3]; // R_Shoulder
    }
    if (w_count >= 5) mapped_joints[7] = whites[4]; // Pelvis (Middle)
    if (w_count >= 7) {
        sort_by_x(whites, 5, 6);
        mapped_joints[9]  = whites[5]; // L_Knee
        mapped_joints[10] = whites[6]; // R_Knee
    }

    // Red marker sorting (2 markers) - Hands
    for (int i = 0; i < r_count; i++) {
        for (int j = i + 1; j < r_count; j++) {
            if (reds[i].x > reds[j].x) swap_joint(&reds[i], &reds[j]);
        }
    }
    if (r_count > 0) mapped_joints[6] = reds[0];             // L_Hand
    if (r_count > 1) mapped_joints[8] = reds[r_count - 1];   // R_Hand

    // Green marker sorting (4 markers) - Elbows, Knees
    for (int i = 0; i < g_count; i++) {
        for (int j = i + 1; j < g_count; j++) {
            if (greens[i].y > greens[j].y) swap_joint(&greens[i], &greens[j]);
        }
    }
    if (g_count >= 2) {
        if (greens[0].x > greens[1].x) swap_joint(&greens[0], &greens[1]);
        mapped_joints[4] = greens[0]; // L_Elbow
        mapped_joints[5] = greens[1]; // R_Elbow
    } else if (g_count == 1) {
        mapped_joints[4] = greens[0];
    }
    if (g_count >= 4) {
        if (greens[2].x > greens[3].x) swap_joint(&greens[2], &greens[3]);
        mapped_joints[11] = greens[2]; // L_Foot
        mapped_joints[12] = greens[3]; // R_Foot
    } else if (g_count == 3) {
        mapped_joints[11] = greens[2];
    }
}

// Skeleton drawing for color mode 
void DrawColorSkeletonModel(u8 *frame, u32 stride, Joint *raw_joints, int *colors, float stretch, int is_erase) {
    Joint mapped_joints[13];
    AssignJointsByColor(raw_joints, colors, mapped_joints);

    // Keep the skeleton connecting lines white (or black for erasing)
    u8 line_r = 255, line_g = 255, line_b = 255;
    if (is_erase) {
        line_r = 0; line_g = 0; line_b = 0;
    }

    for (int i = 0; i < 13; i++) {
        if (mapped_joints[i].x != 0 || mapped_joints[i].y != 0) {
            u8 jr = 0, jg = 0, jb = 0; // Default to black (used for erase)

            if (!is_erase) {
                if (i == 0 || i == 1 || i == 2 || i == 3 || i == 7 || i == 9 || i == 10) {
                    // White markers: Head(0), L_Shoulder(1), Neck(2), R_Shoulder(3), Pelvis(7), Knees(9,10)
                    jr = 255; jg = 255; jb = 255;
                } else if (i == 4 || i == 5 || i == 11 || i == 12) {
                    // Green markers: L_Elbow (4), R_Elbow (5), L_Foot (11), R_Foot (12)
                    jr = 0; jg = 255; jb = 0;
                } else if (i == 6 || i == 8) {
                    // Red markers: L_Hand (6), R_Hand (8)
                    jr = 255; jg = 0; jb = 0;
                }
            }

            int stretched_x = (int)((float)mapped_joints[i].x * stretch);
            draw_joint_marker(frame, stride, stretched_x, mapped_joints[i].y, jr, jg, jb);
        }
    }

    int edges[12][2] = {
        {0,2}, {1,2}, {3,2}, {1,4}, {4,6}, {3,5},
        {5,8}, {2,7}, {7,9}, {9,11}, {7,10}, {10,12}
    };

    for (int i = 0; i < 12; i++) {
        int pt1 = edges[i][0];
        int pt2 = edges[i][1];

        if ((mapped_joints[pt1].x != 0 || mapped_joints[pt1].y != 0) &&
            (mapped_joints[pt2].x != 0 || mapped_joints[pt2].y != 0)) {

            int x0_stretched = (int)((float)mapped_joints[pt1].x * stretch);
            int x1_stretched = (int)((float)mapped_joints[pt2].x * stretch);
            draw_line_optimized(frame, stride, x0_stretched, mapped_joints[pt1].y, x1_stretched, mapped_joints[pt2].y, line_r, line_g, line_b);
        }
    }
}

// Switch between Color or White marker skeleton drawing 
void DrawCharacterModel(u8 *frame, u32 stride, Joint *raw_joints, float stretch, int model_type, int is_erase) {
    if (model_type == 1) {
        DrawColorSkeletonModel(frame, stride, raw_joints, joint_colours, stretch, is_erase);
        return;
    }

    Joint mapped_joints[13];
    JointAssignment(raw_joints, mapped_joints);

    u8 joint_r = 235, joint_g = 52, joint_b = 52;
    u8 line_r = 255, line_g = 255, line_b = 255;

    if (is_erase) {
        joint_r = 0; joint_g = 0; joint_b = 0;
        line_r = 0; line_g = 0; line_b = 0;
    }

    DrawSkeleton(frame, stride, raw_joints, stretch, joint_r, joint_g, joint_b, line_r, line_g, line_b);
}

int abs_int(int x) { return (x < 0) ? -x : x; }

// GUI Functions 

// FPS counter text drawing
void update_fps_string(int fps) {
    if (fps > 99) fps = 99;
    fps_str[0] = (fps / 10) + '0';
    fps_str[1] = (fps % 10) + '0';
}

// 7 segment character drawing 
u8 get_font_row(char c, int row) {
    switch(c) {
        case 'A': { const u8 f[] = {2, 5, 7, 5, 5}; return f[row]; }
        case 'B': { const u8 f[] = {6, 5, 6, 5, 6}; return f[row]; }
        case 'C': { const u8 f[] = {3, 4, 4, 4, 3}; return f[row]; }
        case 'D': { const u8 f[] = {6, 5, 5, 5, 6}; return f[row]; }
        case 'E': { const u8 f[] = {7, 4, 7, 4, 7}; return f[row]; }
        case 'F': { const u8 f[] = {7, 4, 6, 4, 4}; return f[row]; }
        case 'G': { const u8 f[] = {3, 4, 5, 5, 3}; return f[row]; }
        case 'H': { const u8 f[] = {5, 5, 7, 5, 5}; return f[row]; }
        case 'I': { const u8 f[] = {7, 2, 2, 2, 7}; return f[row]; }
        case 'J': { const u8 f[] = {1, 1, 1, 5, 2}; return f[row]; }
        case 'K': { const u8 f[] = {5, 6, 4, 6, 5}; return f[row]; }
        case 'L': { const u8 f[] = {4, 4, 4, 4, 7}; return f[row]; }
        case 'M': { const u8 f[] = {5, 7, 5, 5, 5}; return f[row]; }
        case 'N': { const u8 f[] = {6, 5, 5, 5, 5}; return f[row]; }
        case 'O': { const u8 f[] = {2, 5, 5, 5, 2}; return f[row]; }
        case 'P': { const u8 f[] = {6, 5, 6, 4, 4}; return f[row]; }
        case 'Q': { const u8 f[] = {2, 5, 5, 2, 1}; return f[row]; }
        case 'R': { const u8 f[] = {6, 5, 6, 5, 5}; return f[row]; }
        case 'S': { const u8 f[] = {3, 4, 2, 1, 6}; return f[row]; }
        case 'T': { const u8 f[] = {7, 2, 2, 2, 2}; return f[row]; }
        case 'U': { const u8 f[] = {5, 5, 5, 5, 7}; return f[row]; }
        case 'V': { const u8 f[] = {5, 5, 5, 5, 2}; return f[row]; }
        case 'W': { const u8 f[] = {5, 5, 5, 7, 5}; return f[row]; }
        case 'X': { const u8 f[] = {5, 5, 2, 5, 5}; return f[row]; }
        case 'Y': { const u8 f[] = {5, 5, 2, 2, 2}; return f[row]; }
        case 'Z': { const u8 f[] = {7, 1, 2, 4, 7}; return f[row]; }
        case '0': { const u8 f[] = {7, 5, 5, 5, 7}; return f[row]; }
        case '1': { const u8 f[] = {2, 6, 2, 2, 7}; return f[row]; }
        case '2': { const u8 f[] = {7, 1, 7, 4, 7}; return f[row]; }
        case '3': { const u8 f[] = {7, 1, 7, 1, 7}; return f[row]; }
        case '4': { const u8 f[] = {5, 5, 7, 1, 1}; return f[row]; }
        case '5': { const u8 f[] = {7, 4, 7, 1, 7}; return f[row]; }
        case '6': { const u8 f[] = {7, 4, 7, 5, 7}; return f[row]; }
        case '7': { const u8 f[] = {7, 1, 1, 1, 1}; return f[row]; }
        case '8': { const u8 f[] = {7, 5, 7, 5, 7}; return f[row]; }
        case '9': { const u8 f[] = {7, 5, 7, 1, 7}; return f[row]; }
        case ':': { const u8 f[] = {0, 2, 0, 2, 0}; return f[row]; }
        case ' ': { return 0; }
        default:  return 0;
    }
}

// Draw text function
void DrawMiniText(u8 *frame, u32 stride, int start_x, int start_y, char *str, u8 r, u8 g, u8 b) {
    int scale = 3;
    int cursor_x = start_x;

    while (*str) {
        char c = *str;
        for (int row = 0; row < 5; row++) {
            u8 row_data = get_font_row(c, row);
            for (int col = 0; col < 3; col++) {
                for (int dy = 0; dy < scale; dy++) {
                    for (int dx = 0; dx < scale; dx++) {
                        int px = cursor_x + (col * scale) + dx;
                        int py = start_y + (row * scale) + dy;
                        u32 addr = (py * stride) + (px * 3);

                                                if (row_data & (1 << (2 - col))) {
                                                    frame[addr] = g; frame[addr + 1] = b; frame[addr + 2] = r;
                                                } else {
                                                    frame[addr] = 0; frame[addr + 1] = 0; frame[addr + 2] = 0;
                                                }
                    }
                }
            }
        }
        cursor_x += (4 * scale);
        str++;
    }
}

// Video (V) or Photo (P) mode drawing indicator 
void DrawModeIndicator(u8 *frame, u32 stride, int x, int y, int mode) {
    if (mode == 0) {
        draw_line_optimized(frame, stride, x, y, x + 10, y + 20, 255, 255, 255);
        draw_line_optimized(frame, stride, x + 10, y + 20, x + 20, y, 255, 255, 255);
    } else {
        draw_line_optimized(frame, stride, x, y, x, y + 20, 255, 255, 255);
        draw_line_optimized(frame, stride, x, y, x + 12, y, 255, 255, 255);
        draw_line_optimized(frame, stride, x + 12, y, x + 12, y + 10, 255, 255, 255);
        draw_line_optimized(frame, stride, x + 12, y + 10, x, y + 10, 255, 255, 255);
    }
}

// GUI matching the buttons on the Nexys Video board 
void DrawGUI_Dynamic(u8 *frame, u32 stride, int mode, int paused) {
    int cx = 100, cy = 100;
    int size = 40, gap = 25;

    int box_x[5] = {cx - size - gap, cx, cx + size + gap, cx, cx};
    int box_y[5] = {cy, cy, cy, cy - size - gap, cy + size + gap};

    for (int i = 0; i < 5; i++) {
        u8 r = 50, g = 50, b = 50;
        if (i == 0 && mode == 0) { r = 0; g = 255; b = 0; }
        if (i == 2 && mode == 1) { r = 0; g = 255; b = 0; }
        if (i == 1) {
            if (mode == 0 && paused) { r = 255; g = 0; b = 0; }
            else if (mode == 0 && !paused) { r = 0; g = 255; b = 0; }
            else if (mode == 1) { r = 255; g = 255; b = 255; }
        }

        u32 start_addr = (box_y[i] * stride) + (box_x[i] * 3);

        for (int row = 0; row < size; row++) {
            u32 current_addr = start_addr + (row * stride);
            for (int col = 0; col < size; col++) {
                            frame[current_addr] = g; frame[current_addr + 1] = b; frame[current_addr + 2] = r;
                            current_addr += 3;
                        }
        }
    }

    DrawModeIndicator(frame, stride, box_x[0] + 10, box_y[0] - 25, mode);
}


/* ------------------------------------------------------------ */
/*				Procedure Definitions							*/
/* ------------------------------------------------------------ */

// Color identification functions
// - Look into the framebuffer and match to a color (W, G, R)
#define THRESHOLD 175

volatile int stream_state = 0;
volatile int Marker_state = 0;
volatile int VDMA_state = 0;
volatile int VidS2MM_state = 0;
volatile int DispMM2S_state = 0;
volatile int frame_to_ip = 0;
volatile int output_frame = 1;
uint8_t *CIP_ADDR = CUSTOMIP_BASE_ADDR;
Joint detected_joints[13] = {0};
int joint_colours[13] = {0};
Joint history_joints[DISPLAY_NUM_FRAMES][13] = {0};
int history_colors[DISPLAY_NUM_FRAMES][13] = {0};
int history_model[DISPLAY_NUM_FRAMES] = {0};

// Color IDs
enum {
    COLOR_UNKNOWN = 0,
    COLOR_WHITE,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_PURPLE
};
const char* color_to_string(int color) {
    switch (color) {
        case COLOR_WHITE:  return "WHITE";
        case COLOR_RED:    return "RED";
        case COLOR_GREEN:  return "GREEN";
        case COLOR_YELLOW: return "YELLOW";
        case COLOR_PURPLE: return "PURPLE";
        default:           return "UNKNOWN";
    }
}

void FindColourTarget(u8 *framebuffer, u32 stride, float stretch){
    for (int i = 0; i < 13; i++) {

        int cx = detected_joints[i].x;
        int cy = detected_joints[i].y;

        uint32_t sum_r = 0, sum_g = 0, sum_b = 0;
        int count = 0;

        // 5x5 neighborhood
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {

                int x = cx + dx;
                int y = cy + dy;

                // Bounds check
                if (x >= 0 && x < 1280 && y >= 0 && y < 720)
                    count++;
                else continue;

                uint8_t *pixel = (framebuffer + y * stride + x * 3);
                // if (x==cx && y==cy) xil_printf("Joint %d at (%d, %d): (R=%d, G=%d, B=%d)\n\r",i,cx,cy,pixel[2], pixel[0],pixel[1]);

                sum_r += pixel[2];
                sum_g += pixel[0];
                sum_b += pixel[1];

            }
        }

        if (count == 0) {
            joint_colours[i] = COLOR_UNKNOWN;
            // xil_printf("Joint %d failed\n\r",i);
            continue;
        }

        uint8_t avg_r = sum_r / count;
        uint8_t avg_g = sum_g / count;
        uint8_t avg_b = sum_b / count;

        // Classification
		if (avg_r > THRESHOLD && avg_g > THRESHOLD && avg_b > THRESHOLD)
			joint_colours[i] = COLOR_WHITE;

		else if (avg_r > THRESHOLD && avg_g < THRESHOLD && avg_b < THRESHOLD)
			joint_colours[i] = COLOR_RED;

		else if (avg_r < THRESHOLD && avg_g > THRESHOLD && avg_b < THRESHOLD)
			joint_colours[i] = COLOR_GREEN;

		else if (avg_r > THRESHOLD && avg_g > THRESHOLD && avg_b < THRESHOLD)
			joint_colours[i] = COLOR_YELLOW;

		else if (avg_r > THRESHOLD && avg_g < THRESHOLD && avg_b > THRESHOLD)
			joint_colours[i] = COLOR_PURPLE;

		else
			joint_colours[i] = COLOR_UNKNOWN;

		// Debug print
//		xil_printf("Joint %d at (%d, %d): %s (R=%d, G=%d, B=%d)\n\r",
//			   i, cx, cy,
//			   color_to_string(joint_colours[i]),
//			   avg_r, avg_g, avg_b);

    }
}

// Marker valid output interrupt
void MarkerDetectorISR(void *CallbackRef){
	if(stream_state){
		Marker_state = 1;
	}
}

// Unused VDMA interrupt 
void VidS2MM_ISR(void *CallbackRef){
    if(stream_state){
        VidS2MM_state = 1;
        XIntc_Disable(&intc, VID_S2MM_IRPT_ID);
        return;
    }
}

// VDMA V-sync interrupt 
void DispMM2S_ISR(void *CallbackRef){
    if(stream_state){
        DispMM2S_state = 1;
        XIntc_Disable(&intc, DISP_MM2S_IRPT_ID);
        //xil_printf("Intr\n");
        return;
    }
}


int main(void)
{
	Xil_ICacheEnable();
	Xil_DCacheEnable();

	DemoInitialize();
	DemoRun();

	return 0;
}

void DemoInitialize()
{
	int Status;
	XAxiVdma_Config *vdmaConfig;
	int i;

	for (i = 0; i < DISPLAY_NUM_FRAMES; i++) pFrames[i] = frameBuf[i];

	vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	if (!vdmaConfig) return;

	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS) return;

	Status = DisplayInitialize(&dispCtrl, &vdma, DISP_VTC_ID, DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
	if (Status != XST_SUCCESS) return;

	DisplaySetMode(&dispCtrl, &VMODE_1280x720);
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS) return;

	Status = fnInitInterruptController(&intc);
	if(Status != XST_SUCCESS) return;

	fnEnableInterrupts(&intc, &ivt[0], sizeof(ivt)/sizeof(ivt[0]));

	Status = XIntc_Connect(&intc, XPAR_MICROBLAZE_0_AXI_INTC_MARKER_DETECTER_V4_0_O_VALID_INTR, (XInterruptHandler)MarkerDetectorISR, NULL);
	if (Status != XST_SUCCESS) return;
	XIntc_Enable(&intc, XPAR_MICROBLAZE_0_AXI_INTC_MARKER_DETECTER_V4_0_O_VALID_INTR);

	Status = XIntc_Connect(&intc, XPAR_INTC_0_AXIVDMA_0_S2MM_INTROUT_VEC_ID, (XInterruptHandler)VdmaS2MmISR, NULL);
	if (Status != XST_SUCCESS) return;
	XIntc_Enable(&intc, XPAR_INTC_0_AXIVDMA_0_S2MM_INTROUT_VEC_ID);
	XAxiVdma_IntrEnable(&vdma, XAXIVDMA_IXR_COMPLETION_MASK, XAXIVDMA_WRITE);
	XAxiVdma_IntrEnable(&vdma, XAXIVDMA_IXR_COMPLETION_MASK, XAXIVDMA_READ);

	Status = XIntc_Connect(&intc,
		                       VID_S2MM_IRPT_ID,
		                       (XInterruptHandler)VidS2MM_ISR,
		                       NULL);
	if (Status != XST_SUCCESS) {
		xil_printf("Marker intr connect failed\r\n");
		return XST_FAILURE;
	}
	//XIntc_Enable(&intc, VID_S2MM_IRPT_ID);
	XIntc_Disable(&intc, VID_S2MM_IRPT_ID);

	Status = XIntc_Connect(&intc,
						   DISP_MM2S_IRPT_ID,
						   (XInterruptHandler)DispMM2S_ISR,
						   NULL);
	if (Status != XST_SUCCESS) {
		xil_printf("Marker intr connect failed\r\n");
		return XST_FAILURE;
	}
	//XIntc_Enable(&intc, DISP_MM2S_IRPT_ID);
	XIntc_Disable(&intc, DISP_MM2S_IRPT_ID);

	Status = VideoInitialize(&videoCapt, &intc, &vdma, VID_GPIO_ID, VID_VTC_ID, VID_VTC_IRPT_ID, pFrames, DEMO_STRIDE, DEMO_START_ON_DET);
	if (Status != XST_SUCCESS) return;

    Status = XTmrCtr_Initialize(&TimerInst, SCU_TIMER_ID);

    Status = XGpio_Initialize(&DpadGpio, D_PAD_GPIO_ID);
    if (Status == XST_SUCCESS) {
        XGpio_SetDataDirection(&DpadGpio, 1, 0xFFFFFFFF);
        xil_printf("D-pad successfully initialized at 0x40010000!\r\n");
    }

	VideoSetCallback(&videoCapt, DemoISR, &fRefresh);

	usleep(500000);

	if (videoCapt.state == VIDEO_STREAMING || videoCapt.state == VIDEO_PAUSED) {
	    VideoStart(&videoCapt);
	} else {
	    DemoPrintTest(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, DEMO_PATTERN_1);
	}

	u32 current_frame_addr = (u32)pFrames[0];
	if (init_ip_vdma(current_frame_addr, 1280, 720, dispCtrl.stride) != XST_SUCCESS) {
			xil_printf("IP VDMA failed to start!\r\n");
	}

	return;
}

void DemoRun()
{
	int nextFrame = 0;
	char userInput = 0;
	u32 locked;
	XGpio *GpioPtr = &videoCapt.gpio;

	while (!XUartLite_IsReceiveEmpty(UART_BASEADDR)) XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);

	while (userInput != 'q')
	{
		fRefresh = 0;
		DemoPrintMenu();

		while (XUartLite_IsReceiveEmpty(UART_BASEADDR) && !fRefresh) {}

		if (!XUartLite_IsReceiveEmpty(UART_BASEADDR)) {
			userInput = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
			xil_printf("%c", userInput);
		} else {
			userInput = 'r';
		}

		switch (userInput)
		{
		case '1': DemoChangeRes(); break;
		case '2':
			nextFrame = dispCtrl.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES) nextFrame = 0;
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case '3': DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_0); break;
		case '4': DemoPrintTest(pFrames[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE, DEMO_PATTERN_1); break;
		case '5':
			if (videoCapt.state == VIDEO_STREAMING) VideoStop(&videoCapt);
			else VideoStart(&videoCapt);
			break;
		case '6':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES) nextFrame = 0;
			VideoChangeFrame(&videoCapt, nextFrame);
			break;
		case '7':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES) nextFrame = 0;
			VideoStop(&videoCapt);
			DemoInvertFrame(pFrames[videoCapt.curFrame], pFrames[nextFrame], videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, DEMO_STRIDE);
			VideoStart(&videoCapt);
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case '8':
			nextFrame = videoCapt.curFrame + 1;
			if (nextFrame >= DISPLAY_NUM_FRAMES) nextFrame = 0;
			VideoStop(&videoCapt);
			DemoScaleFrame(pFrames[videoCapt.curFrame], pFrames[nextFrame], videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo, dispCtrl.vMode.width, dispCtrl.vMode.height, DEMO_STRIDE);
			VideoStart(&videoCapt);
			DisplayChangeFrame(&dispCtrl, nextFrame);
			break;
		case 'f':
			stream_state = 1;
			DisplayChangeFrame(&dispCtrl, 1);
			while(1){
				if (!XUartLite_IsReceiveEmpty(UART_BASEADDR)) break;
				*(CIP_ADDR+15) = 1;
				while(!Marker_state) {}
				Marker_state = 0;
			}
			stream_state = 0;
            break;
		case 's':
		    if (videoCapt.state == VIDEO_STREAMING) {
		        VideoStop(&videoCapt);

		        u32 write_bit = 1;
		        *(CIP_ADDR + 15) = write_bit;
		        usleep(150000);

		        read_ip_registers(detected_joints);
		        FindColourTarget(pFrames[0],DEMO_STRIDE, 1.00f);

		        nextFrame = dispCtrl.curFrame + 1;
		        if (nextFrame >= DISPLAY_NUM_FRAMES) nextFrame = 0;

		        volatile uint8_t* frbuf = pFrames[nextFrame];

		        Xil_DCacheInvalidateRange((unsigned int)pFrames[videoCapt.curFrame], DEMO_MAX_FRAME);
		        memcpy(pFrames[nextFrame], pFrames[videoCapt.curFrame], DEMO_MAX_FRAME);

                DrawCharacterModel(pFrames[nextFrame], dispCtrl.stride, detected_joints, 1.00f, current_model, 0);
		        DisplayChangeFrame(&dispCtrl, nextFrame);
                last_drawn_model = current_model;

		        xil_printf("\n\rSkeleton drawn, 'c' to restart video stream\n\r");
		        usleep(1500000);

		    } else {
		        xil_printf("\n\r\n\r'c'\n\r");
		        usleep(1000000);
		    }
		    break;
		case 'c':
			if (videoCapt.state != VIDEO_STREAMING) {
				VideoStart(&videoCapt);
				DisplayChangeFrame(&dispCtrl, videoCapt.curFrame);
				xil_printf("\n\r\n\rVideo stream resumed\n\r");
			} else {
				DisplayChangeFrame(&dispCtrl, videoCapt.curFrame);
			}
			usleep(500000);
			break;
        // GUI MODE -> USE THIS 
		case 'v':
					xil_printf("DPAD enabled\r\n");
		            XTmrCtr_Start(&TimerInst, 0);
		            last_timer_val = XTmrCtr_GetValue(&TimerInst, 0);
		            frame_count = 0;

		            memset(pFrames[0], 0, DEMO_MAX_FRAME);
		            memset(pFrames[1], 0, DEMO_MAX_FRAME);
		            memset(pFrames[2], 0, DEMO_MAX_FRAME);

		            int gui_needs_update = 1;
		            vdma_frame_done = 0;

		            while (1) {
		                if (!XUartLite_IsReceiveEmpty(UART_BASEADDR)) {
		                    stream_state = 0;
		                    break;
		                }

		                u32 btn_state = XGpio_DiscreteRead(&DpadGpio, 1);

		                if (btn_state != 0 && button_debounced) {
		                    button_debounced = 0;
		                    gui_needs_update = 1;

		                    if (btn_state & 0x10) { // TOP (Exit)
		                        xil_printf("Top - Exit GUI \r\n");
		                        stream_state = 0;
		                        DrawCharacterModel(pFrames[1], dispCtrl.stride, detected_joints, 1.00f, last_drawn_model, 1);
		                        Xil_DCacheFlushRange((unsigned int)pFrames[1], DEMO_MAX_FRAME);
		                        DrawCharacterModel(pFrames[dispCtrl.curFrame], dispCtrl.stride, detected_joints, 1.00f, last_drawn_model, 1);
		                        Xil_DCacheFlushRange((unsigned int)pFrames[dispCtrl.curFrame], DEMO_MAX_FRAME);
		                        VideoStart(&videoCapt);
		                        video_paused = 0;
		                        photo_captured = 0;
		                        DisplayChangeFrame(&dispCtrl, videoCapt.curFrame);
		                        break;
		                    }
		                    else if (btn_state & 0x02) { // BOTTOM (Cycle Models)
		                        current_model = (current_model + 1) % MAX_MODELS;
		                        xil_printf("Switched to model: %d\r\n", current_model);

		                        // If Paused, clear to black to erase cleanly, then redraw
		                        if (current_mode == 0 && video_paused) {
		                            memset(pFrames[1], 0, DEMO_MAX_FRAME);
		                            DrawCharacterModel(pFrames[1], dispCtrl.stride, detected_joints, 1.00f, current_model, 0);
		                            last_drawn_model = current_model;
		                        }
		                        // If photo captured, clear to black to erase cleanly
		                        else if (current_mode == 1 && photo_captured) {
		                            memset(pFrames[dispCtrl.curFrame], 0, DEMO_MAX_FRAME);
		                            last_drawn_model = current_model;
		                        }
		                    }
		                    else if (btn_state & 0x04) { // LEFT (Video Mode)
		                        if (current_mode != 0) {
		                            current_mode = 0;
		                            DrawCharacterModel(pFrames[dispCtrl.curFrame], dispCtrl.stride, detected_joints, 1.00f, last_drawn_model, 1);
		                            Xil_DCacheFlushRange((unsigned int)pFrames[dispCtrl.curFrame], DEMO_MAX_FRAME);
		                            if (photo_captured) {
		                                VideoStart(&videoCapt);
		                                photo_captured = 0;
		                            }
		                        }
		                    }
		                    else if (btn_state & 0x08) { // RIGHT (Photo Mode)
		                        if (current_mode != 1) {
		                            current_mode = 1;
		                            stream_state = 0;
		                            DrawCharacterModel(pFrames[1], dispCtrl.stride, detected_joints, 1.00f, last_drawn_model, 1);
		                            Xil_DCacheFlushRange((unsigned int)pFrames[1], DEMO_MAX_FRAME);
		                            if (video_paused) {
		                                VideoStart(&videoCapt);
		                                video_paused = 0;
		                            }
		                            DisplayChangeFrame(&dispCtrl, videoCapt.curFrame);
		                        }
		                    }
		                    else if (btn_state & 0x01) { // CENTER
		                        if (current_mode == 0) {
		                            video_paused = !video_paused;
		                            if (video_paused) {
		                                stream_state = 0;
		                                VideoStop(&videoCapt);
		                            } else {
		                                VideoStart(&videoCapt);
		                            }
		                        } else {
		                            if (photo_captured == 0) {
		                                VideoStop(&videoCapt);
		                                *(CIP_ADDR + 15) = 1;
		                                usleep(150000);

		                                // Get coordinates from IP
		                                read_ip_registers(detected_joints);

		                                FindColourTarget(pFrames[videoCapt.curFrame], dispCtrl.stride, 1.00f);

		                                photo_captured = 1;
		                            } else {
		                                VideoStart(&videoCapt);
		                                photo_captured = 0;
		                            }
		                        }
		                    }
		                }
		                else if (btn_state == 0) {
		                    button_debounced = 1;
		                }

                        // FPS Counter
		                u32 current_timer_val = XTmrCtr_GetValue(&TimerInst, 0);

		                if (current_timer_val >= 100000000) {
		                    current_fps = frame_count;
		                    update_fps_string(current_fps);
		                    frame_count = 0;
		                    XTmrCtr_Reset(&TimerInst, 0);

		                    gui_needs_update = 1;
		                }

		                int active_frame = dispCtrl.curFrame;

		                if (current_mode == 0) {
		                	if (!video_paused) {
                                // Video mode loop
		                	    stream_state = 1;

                                // Double buffering, choose an unused frame 
		                	    int work_frame = (dispCtrl.curFrame + 1) % DISPLAY_NUM_FRAMES;
		                	    if (work_frame == videoCapt.curFrame && DISPLAY_NUM_FRAMES > 2) {
		                	        work_frame = (work_frame + 1) % DISPLAY_NUM_FRAMES;
		                	    }

		                	    *(CIP_ADDR+15) = 1; // Enable Custom IP 
		                	    while(!Marker_state) {}
		                	    Marker_state = 0;

		                	        Xil_DCacheInvalidateRange((unsigned int)pFrames[videoCapt.curFrame], DEMO_MAX_FRAME); // Invalidate previous image 

		                	        if (history_model[work_frame] == 1) { // Store history for fast erasing 
		                	            DrawColorSkeletonModel(pFrames[work_frame], dispCtrl.stride, history_joints[work_frame], history_colors[work_frame], 1.00f, 1);
		                	        } else {
		                	            DrawCharacterModel(pFrames[work_frame], dispCtrl.stride, history_joints[work_frame], 1.00f, history_model[work_frame], 1);
		                	        }

		                	        read_ip_registers(detected_joints);

		                	        FindColourTarget(pFrames[videoCapt.curFrame], dispCtrl.stride, 1.00f);

		                	        if (current_model == 1) {
		                	            DrawColorSkeletonModel(pFrames[work_frame], dispCtrl.stride, detected_joints, joint_colours, 1.00f, 0);
		                	        } else {
		                	            DrawCharacterModel(pFrames[work_frame], dispCtrl.stride, detected_joints, 1.00f, current_model, 0);
		                	        }
		                	        last_drawn_model = current_model;

		                	        for (int i = 0; i < 13; i++) {
		                	            history_joints[work_frame][i] = detected_joints[i];
		                	            history_colors[work_frame][i] = joint_colours[i];
		                	        }
		                	        history_model[work_frame] = current_model;

		                	        DrawGUI_Dynamic(pFrames[work_frame], dispCtrl.stride, current_mode, 0);
		                	        DrawMiniText(pFrames[work_frame], dispCtrl.stride, 20, 300, fps_str, 255, 0, 0);

		                	        Xil_DCacheFlushRange((unsigned int)pFrames[work_frame], DEMO_MAX_FRAME); // Flush MBprocessor cache to get the image into memory

		                	        XIntc_Enable(&intc, DISP_MM2S_IRPT_ID); 
		                	        while(!DispMM2S_state){} // Wait for previous frame Vsync then switch to new frame 
		                	        DispMM2S_state = 0;

		                	        DisplayChangeFrame(&dispCtrl, work_frame);

		                	        frame_count++;

		                	} else {
		                        if (gui_needs_update) {
		                            DrawGUI_Dynamic(pFrames[1], dispCtrl.stride, current_mode, 1);
		                            DrawMiniText(pFrames[1], dispCtrl.stride, 20, 300, fps_str, 255, 0, 0);
		                            Xil_DCacheFlushRange((unsigned int)pFrames[1], DEMO_MAX_FRAME);
		                            gui_needs_update = 0;
		                        }
		                        usleep(10000);
		                    }
		                }
		                else if (current_mode == 1) {
                            // Photo mode 
		                    if (!photo_captured) {
		                        if (vdma_frame_done) {
		                            vdma_frame_done = 0;
		                            frame_count++;
		                            active_frame = videoCapt.curFrame;
		                            DisplayChangeFrame(&dispCtrl, active_frame);
		                            Xil_DCacheInvalidateRange((unsigned int)pFrames[active_frame], DEMO_MAX_FRAME);

		                            DrawGUI_Dynamic(pFrames[active_frame], dispCtrl.stride, current_mode, 0);
		                            DrawMiniText(pFrames[active_frame], dispCtrl.stride, 20, 300, fps_str, 255, 0, 0);
		                            Xil_DCacheFlushRange((unsigned int)pFrames[active_frame], DEMO_MAX_FRAME);
		                        }
		                    } else {
		                        if (gui_needs_update) {
		                            DrawCharacterModel(pFrames[active_frame], dispCtrl.stride, detected_joints, 1.00f, current_model, 0);
		                            DrawGUI_Dynamic(pFrames[active_frame], dispCtrl.stride, current_mode, 1);
		                            DrawMiniText(pFrames[active_frame], dispCtrl.stride, 20, 300, fps_str, 255, 0, 0);

		                            Xil_DCacheFlushRange((unsigned int)pFrames[active_frame], DEMO_MAX_FRAME);
		                            gui_needs_update = 0;
		                        }
		                        usleep(10000);
		                    }
		                }
		            }
					break;
		case 'q': break;
		case 'r':
			locked = XGpio_DiscreteRead(GpioPtr, 2);
			xil_printf("%d", locked);
			break;
		default :
			xil_printf("\n\rInvalid Selection");
			usleep(50000);
		}
	}

	return;
}

void DemoPrintMenu()
{
	xil_printf("\x1B[H\x1B[2J");
	xil_printf("**************************************************\n\r");
	xil_printf("* Nexys Video HDMI Demo              *\n\r");
	xil_printf("**************************************************\n\r");
	xil_printf("*Display Resolution: %28s*\n\r", dispCtrl.vMode.label);
	printf("*Display Pixel Clock Freq. (MHz): %15.3f*\n\r", dispCtrl.pxlFreq);
	xil_printf("*Display Frame Index: %27d*\n\r", dispCtrl.curFrame);
	if (videoCapt.state == VIDEO_DISCONNECTED) xil_printf("*Video Capture Resolution: %22s*\n\r", "!HDMI UNPLUGGED!");
	else xil_printf("*Video Capture Resolution: %17dx%-4d*\n\r", videoCapt.timing.HActiveVideo, videoCapt.timing.VActiveVideo);
	xil_printf("*Video Frame Index: %29d*\n\r", videoCapt.curFrame);
	xil_printf("**************************************************\n\r");
	xil_printf("\n\r1 - Change Display Resolution\n\r2 - Change Display Framebuffer Index\n\r");
	xil_printf("3 - Print Blended Test Pattern to Display Framebuffer\n\r4 - Print Color Bar Test Pattern to Display Framebuffer\n\r");
	xil_printf("5 - Start/Stop Video stream into Video Framebuffer\n\r6 - Change Video Framebuffer Index\n\r");
	xil_printf("7 - Grab Video Frame and invert colors\n\r8 - Grab Video Frame and scale to Display resolution\n\r");
	xil_printf("s - INSTANT SCREENCAP (Software Anatomical Skeleton)\n\r");
	xil_printf("c - Clear Screencap and Resume Live Video\n\r");
	xil_printf("v - DPAD GUI Mode\n\r");
	xil_printf("q - Quit\n\r\n\r\n\rEnter a selection:");
}

void DemoChangeRes()
{
	int fResSet = 0, status;
	char userInput = 0;

	while (!XUartLite_IsReceiveEmpty(UART_BASEADDR)) XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);

	while (!fResSet) {
		DemoCRMenu();
		while (XUartLite_IsReceiveEmpty(UART_BASEADDR) && !fRefresh) {}
		userInput = XUartLite_ReadReg(UART_BASEADDR, XUL_RX_FIFO_OFFSET);
		xil_printf("%c", userInput);
		status = XST_SUCCESS;
		switch (userInput) {
		case '1': status = DisplayStop(&dispCtrl); DisplaySetMode(&dispCtrl, &VMODE_640x480); DisplayStart(&dispCtrl); fResSet = 1; break;
		case '2': status = DisplayStop(&dispCtrl); DisplaySetMode(&dispCtrl, &VMODE_800x600); DisplayStart(&dispCtrl); fResSet = 1; break;
		case '3': status = DisplayStop(&dispCtrl); DisplaySetMode(&dispCtrl, &VMODE_1280x720); DisplayStart(&dispCtrl); fResSet = 1; break;
		case '4': status = DisplayStop(&dispCtrl); DisplaySetMode(&dispCtrl, &VMODE_1280x1024); DisplayStart(&dispCtrl); fResSet = 1; break;
		case '5': status = DisplayStop(&dispCtrl); DisplaySetMode(&dispCtrl, &VMODE_1920x1080); DisplayStart(&dispCtrl); fResSet = 1; break;
		case 'q': fResSet = 1; break;
		default: xil_printf("\n\rInvalid Selection"); usleep(50000);
		}
		if (status == XST_DMA_ERROR) xil_printf("\n\rWARNING: AXI VDMA Error detected and cleared\n\r");
	}
}

void DemoCRMenu()
{
	xil_printf("\x1B[H\x1B[2J");
	xil_printf("**************************************************\n\r* Nexys Video HDMI Demo              *\n\r");
	xil_printf("**************************************************\n\r*Current Resolution: %28s*\n\r", dispCtrl.vMode.label);
	printf("*Pixel Clock Freq. (MHz): %23.3f*\n\r**************************************************\n\r\n\r", dispCtrl.pxlFreq);
	xil_printf("1 - %s\n\r2 - %s\n\r3 - %s\n\r4 - %s\n\r5 - %s\n\r", VMODE_640x480.label, VMODE_800x600.label, VMODE_1280x720.label, VMODE_1280x1024.label, VMODE_1920x1080.label);
	xil_printf("q - Quit (don't change resolution)\n\r\n\rSelect a new resolution:");
}

void DemoInvertFrame(u8 *srcFrame, u8 *destFrame, u32 width, u32 height, u32 stride)
{
	u32 xcoi, ycoi, lineStart = 0;
	for(ycoi = 0; ycoi < height; ycoi++) {
		for(xcoi = 0; xcoi < (width * 3); xcoi+=3) {
			destFrame[xcoi + lineStart] = ~srcFrame[xcoi + lineStart];
			destFrame[xcoi + lineStart + 1] = ~srcFrame[xcoi + lineStart + 1];
			destFrame[xcoi + lineStart + 2] = ~srcFrame[xcoi + lineStart + 2];
		}
		lineStart += stride;
	}
	Xil_DCacheFlushRange((unsigned int) destFrame, DEMO_MAX_FRAME);
}

void DemoScaleFrame(u8 *srcFrame, u8 *destFrame, u32 srcWidth, u32 srcHeight, u32 destWidth, u32 destHeight, u32 stride)
{
	float xInc, yInc, xcoSrc, ycoSrc, x1y1, x2y1, x1y2, x2y2, xDist, yDist;
	int ix1y1, ix2y1, ix1y2, ix2y2, xcoDest, ycoDest, iy1, iDest, i;

	xInc = ((float) srcWidth - 1.0) / ((float) destWidth);
	yInc = ((float) srcHeight - 1.0) / ((float) destHeight);
	ycoSrc = 0.0;
	for (ycoDest = 0; ycoDest < destHeight; ycoDest++) {
		iy1 = ((int) ycoSrc) * stride;
		yDist = ycoSrc - ((float) ((int) ycoSrc));
		iDest = ycoDest * stride;
		xcoSrc = 0.0;
		for (xcoDest = 0; xcoDest < destWidth; xcoDest++) {
			ix1y1 = iy1 + ((int) xcoSrc) * 3;
			ix2y1 = ix1y1 + 3;
			ix1y2 = ix1y1 + stride;
			ix2y2 = ix1y1 + stride + 3;
			xDist = xcoSrc - ((float) ((int) xcoSrc));
			for (i = 0; i < 3; i++) {
				x1y1 = (float) srcFrame[ix1y1 + i]; x2y1 = (float) srcFrame[ix2y1 + i];
				x1y2 = (float) srcFrame[ix1y2 + i]; x2y2 = (float) srcFrame[ix2y2 + i];
				destFrame[iDest] = (u8) ((1.0-yDist)*((1.0-xDist)*x1y1+xDist*x2y1) + yDist*((1.0-xDist)*x1y2+xDist*x2y2));
				iDest++;
			}
			xcoSrc += xInc;
		}
		ycoSrc += yInc;
	}
	Xil_DCacheFlushRange((unsigned int) destFrame, DEMO_MAX_FRAME);
}

void DemoPrintTest(u8 *frame, u32 width, u32 height, u32 stride, int pattern)
{
	u32 xcoi, ycoi, iPixelAddr, wCurrentInt, xLeft, xMid, xRight, xInt, yMid, yInt;
	u8 wRed, wBlue, wGreen;
	double fRed, fBlue, fGreen, fColor, xInc, yInc;

	switch (pattern) {
	case DEMO_PATTERN_0:
		xInt = width / 4; xLeft = xInt * 3; xMid = xInt * 2 * 3; xRight = xInt * 3 * 3; xInc = 256.0 / ((double) xInt);
		yInt = height / 2; yMid = yInt; yInc = 256.0 / ((double) yInt);
		fBlue = 0.0; fRed = 256.0;
		for(xcoi = 0; xcoi < (width*3); xcoi+=3) {
			wRed = (fRed >= 256.0) ? 255 : ((u8) fRed); wBlue = (fBlue >= 256.0) ? 255 : ((u8) fBlue);
			iPixelAddr = xcoi; fGreen = 0.0;
			for(ycoi = 0; ycoi < height; ycoi++) {
				wGreen = (fGreen >= 256.0) ? 255 : ((u8) fGreen);
				frame[iPixelAddr] = wBlue; frame[iPixelAddr + 1] = wGreen; frame[iPixelAddr + 2] = wRed;
				if (ycoi < yMid) fGreen += yInc; else fGreen -= yInc;
				iPixelAddr += stride;
			}
			if (xcoi < xLeft) { fBlue = 0.0; fRed -= xInc; }
			else if (xcoi < xMid) { fBlue += xInc; fRed += xInc; }
			else if (xcoi < xRight) { fBlue -= xInc; fRed -= xInc; }
			else { fBlue += xInc; fRed = 0; }
		}
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	case DEMO_PATTERN_1:
		xInt = width / 7; xInc = 256.0 / ((double) xInt);
		fColor = 0.0; wCurrentInt = 1;
		for(xcoi = 0; xcoi < (width*3); xcoi+=3) {
			if (wCurrentInt > 7) { wRed = 255; wBlue = 255; wGreen = 255; }
			else {
				wRed = (wCurrentInt & 0b001) ? (u8) fColor : 0;
				wBlue = (wCurrentInt & 0b010) ? (u8) fColor : 0;
				wGreen = (wCurrentInt & 0b100) ? (u8) fColor : 0;
			}
			iPixelAddr = xcoi;
			for(ycoi = 0; ycoi < height; ycoi++) {
				frame[iPixelAddr] = wBlue; frame[iPixelAddr + 1] = wGreen; frame[iPixelAddr + 2] = wRed;
				iPixelAddr += stride;
			}
			fColor += xInc;
			if (fColor >= 256.0) { fColor = 0.0; wCurrentInt++; }
		}
		Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
		break;
	default: xil_printf("Error: invalid pattern passed to DemoPrintTest");
	}
}

void DemoISR(void *callBackRef, void *pVideo)
{
	char *data = (char *) callBackRef;
	*data = 1;
}
