# Real-time-2D-Motion-Capture-System
Real-time 2D Motion Capture System implemented on the Nexys Video Board. 

Video demonstration [Google Drive Link](https://drive.google.com/file/d/1Yzdu2OfsamZrElvRkCOrUarLX_b6eZWN/view)

---

## Contents 
- Custom IP Verilog code, including testbenches used to simulate the Custom IP for real images.
- Software: the Vitis 2024.1 project is included, along with a standalone C file that contains all of the code for the Microblaze Processor.
- Hardware: the Vivado 2024.1 project that was used can be recreated using the tcl script and the list of custom IPs. 
- Final report and presentation PDF. 

---


```
Real-time-2D-Motion-Capture-System/
├── Final Report - Group 9.pdf         # Final report PDF 
├── Custom IP Verilog/                 # List of Verilog files for the Custom IP 
│   ├── axi_slave_lite.v               # AXI interface
│   ├── custom_ip_top.v                # Custom IP top module
│   ├── joint_detect.v                 # Marker detection
│   └── test/                          # Testbenches used for the Custom IP 
│       ├── img_to_tb.py
│       ├── input.png
│       ├── joints_to_img.py
│       └── sim_id.sv 
├── Microblaze C Code/                 # C code for Microblaze Processor 
│   └── full_video_532.c               
├── Vitis Classic 2024.1 Project/      # Vitis Classic 2024.1 Software Project File
│   └── Group9_SW.zip
├── Vivado 2024.1 Project/             # Vivado 2024.1 Hardware Project Files (can rebuild the project using these files) 
│   ├── IP_Repo/                       # IP Folder 
│   ├── Nexys-Video-HW1/
│   ├── marker_detector_ip/            # Custom IP source code folder
│   └── Nexys-Video-HW.tcl             # .tcl file used to rebuild the project 
└── README.md
```
