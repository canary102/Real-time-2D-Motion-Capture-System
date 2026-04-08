# Definitional proc to organize widgets for parameters.
proc init_gui { IPINST } {
  ipgui::add_param $IPINST -name "Component_Name"
  #Adding Page
  set Page_0 [ipgui::add_page $IPINST -name "Page 0"]
  ipgui::add_param $IPINST -name "C_S00_AXIS_TDATA_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_S00_AXI_ADDR_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "C_S00_AXI_DATA_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "MAX_CIRCLES" -parent ${Page_0}
  ipgui::add_param $IPINST -name "MAX_GAP" -parent ${Page_0}
  ipgui::add_param $IPINST -name "MIN_HEIGHT" -parent ${Page_0}
  ipgui::add_param $IPINST -name "MIN_WIDTH" -parent ${Page_0}
  ipgui::add_param $IPINST -name "THRESHOLD_G" -parent ${Page_0}
  ipgui::add_param $IPINST -name "THRESHOLD_R_B" -parent ${Page_0}
  ipgui::add_param $IPINST -name "TOTAL_THRESHOLD" -parent ${Page_0}
  ipgui::add_param $IPINST -name "X_BIT_SIZE" -parent ${Page_0}
  ipgui::add_param $IPINST -name "X_SIZE" -parent ${Page_0}
  ipgui::add_param $IPINST -name "Y_BIT_SIZE" -parent ${Page_0}
  ipgui::add_param $IPINST -name "Y_SIZE" -parent ${Page_0}


}

proc update_PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH { PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH } {
	# Procedure called to update C_S00_AXIS_TDATA_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH { PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH } {
	# Procedure called to validate C_S00_AXIS_TDATA_WIDTH
	return true
}

proc update_PARAM_VALUE.C_S00_AXI_ADDR_WIDTH { PARAM_VALUE.C_S00_AXI_ADDR_WIDTH } {
	# Procedure called to update C_S00_AXI_ADDR_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_S00_AXI_ADDR_WIDTH { PARAM_VALUE.C_S00_AXI_ADDR_WIDTH } {
	# Procedure called to validate C_S00_AXI_ADDR_WIDTH
	return true
}

proc update_PARAM_VALUE.C_S00_AXI_DATA_WIDTH { PARAM_VALUE.C_S00_AXI_DATA_WIDTH } {
	# Procedure called to update C_S00_AXI_DATA_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.C_S00_AXI_DATA_WIDTH { PARAM_VALUE.C_S00_AXI_DATA_WIDTH } {
	# Procedure called to validate C_S00_AXI_DATA_WIDTH
	return true
}

proc update_PARAM_VALUE.MAX_CIRCLES { PARAM_VALUE.MAX_CIRCLES } {
	# Procedure called to update MAX_CIRCLES when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.MAX_CIRCLES { PARAM_VALUE.MAX_CIRCLES } {
	# Procedure called to validate MAX_CIRCLES
	return true
}

proc update_PARAM_VALUE.MAX_GAP { PARAM_VALUE.MAX_GAP } {
	# Procedure called to update MAX_GAP when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.MAX_GAP { PARAM_VALUE.MAX_GAP } {
	# Procedure called to validate MAX_GAP
	return true
}

proc update_PARAM_VALUE.MIN_HEIGHT { PARAM_VALUE.MIN_HEIGHT } {
	# Procedure called to update MIN_HEIGHT when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.MIN_HEIGHT { PARAM_VALUE.MIN_HEIGHT } {
	# Procedure called to validate MIN_HEIGHT
	return true
}

proc update_PARAM_VALUE.MIN_WIDTH { PARAM_VALUE.MIN_WIDTH } {
	# Procedure called to update MIN_WIDTH when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.MIN_WIDTH { PARAM_VALUE.MIN_WIDTH } {
	# Procedure called to validate MIN_WIDTH
	return true
}

proc update_PARAM_VALUE.THRESHOLD_G { PARAM_VALUE.THRESHOLD_G } {
	# Procedure called to update THRESHOLD_G when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.THRESHOLD_G { PARAM_VALUE.THRESHOLD_G } {
	# Procedure called to validate THRESHOLD_G
	return true
}

proc update_PARAM_VALUE.THRESHOLD_R_B { PARAM_VALUE.THRESHOLD_R_B } {
	# Procedure called to update THRESHOLD_R_B when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.THRESHOLD_R_B { PARAM_VALUE.THRESHOLD_R_B } {
	# Procedure called to validate THRESHOLD_R_B
	return true
}

proc update_PARAM_VALUE.TOTAL_THRESHOLD { PARAM_VALUE.TOTAL_THRESHOLD } {
	# Procedure called to update TOTAL_THRESHOLD when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.TOTAL_THRESHOLD { PARAM_VALUE.TOTAL_THRESHOLD } {
	# Procedure called to validate TOTAL_THRESHOLD
	return true
}

proc update_PARAM_VALUE.X_BIT_SIZE { PARAM_VALUE.X_BIT_SIZE } {
	# Procedure called to update X_BIT_SIZE when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.X_BIT_SIZE { PARAM_VALUE.X_BIT_SIZE } {
	# Procedure called to validate X_BIT_SIZE
	return true
}

proc update_PARAM_VALUE.X_SIZE { PARAM_VALUE.X_SIZE } {
	# Procedure called to update X_SIZE when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.X_SIZE { PARAM_VALUE.X_SIZE } {
	# Procedure called to validate X_SIZE
	return true
}

proc update_PARAM_VALUE.Y_BIT_SIZE { PARAM_VALUE.Y_BIT_SIZE } {
	# Procedure called to update Y_BIT_SIZE when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.Y_BIT_SIZE { PARAM_VALUE.Y_BIT_SIZE } {
	# Procedure called to validate Y_BIT_SIZE
	return true
}

proc update_PARAM_VALUE.Y_SIZE { PARAM_VALUE.Y_SIZE } {
	# Procedure called to update Y_SIZE when any of the dependent parameters in the arguments change
}

proc validate_PARAM_VALUE.Y_SIZE { PARAM_VALUE.Y_SIZE } {
	# Procedure called to validate Y_SIZE
	return true
}


proc update_MODELPARAM_VALUE.X_SIZE { MODELPARAM_VALUE.X_SIZE PARAM_VALUE.X_SIZE } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.X_SIZE}] ${MODELPARAM_VALUE.X_SIZE}
}

proc update_MODELPARAM_VALUE.X_BIT_SIZE { MODELPARAM_VALUE.X_BIT_SIZE PARAM_VALUE.X_BIT_SIZE } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.X_BIT_SIZE}] ${MODELPARAM_VALUE.X_BIT_SIZE}
}

proc update_MODELPARAM_VALUE.Y_SIZE { MODELPARAM_VALUE.Y_SIZE PARAM_VALUE.Y_SIZE } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.Y_SIZE}] ${MODELPARAM_VALUE.Y_SIZE}
}

proc update_MODELPARAM_VALUE.Y_BIT_SIZE { MODELPARAM_VALUE.Y_BIT_SIZE PARAM_VALUE.Y_BIT_SIZE } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.Y_BIT_SIZE}] ${MODELPARAM_VALUE.Y_BIT_SIZE}
}

proc update_MODELPARAM_VALUE.THRESHOLD_R_B { MODELPARAM_VALUE.THRESHOLD_R_B PARAM_VALUE.THRESHOLD_R_B } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.THRESHOLD_R_B}] ${MODELPARAM_VALUE.THRESHOLD_R_B}
}

proc update_MODELPARAM_VALUE.THRESHOLD_G { MODELPARAM_VALUE.THRESHOLD_G PARAM_VALUE.THRESHOLD_G } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.THRESHOLD_G}] ${MODELPARAM_VALUE.THRESHOLD_G}
}

proc update_MODELPARAM_VALUE.MAX_CIRCLES { MODELPARAM_VALUE.MAX_CIRCLES PARAM_VALUE.MAX_CIRCLES } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.MAX_CIRCLES}] ${MODELPARAM_VALUE.MAX_CIRCLES}
}

proc update_MODELPARAM_VALUE.MIN_HEIGHT { MODELPARAM_VALUE.MIN_HEIGHT PARAM_VALUE.MIN_HEIGHT } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.MIN_HEIGHT}] ${MODELPARAM_VALUE.MIN_HEIGHT}
}

proc update_MODELPARAM_VALUE.MAX_GAP { MODELPARAM_VALUE.MAX_GAP PARAM_VALUE.MAX_GAP } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.MAX_GAP}] ${MODELPARAM_VALUE.MAX_GAP}
}

proc update_MODELPARAM_VALUE.MIN_WIDTH { MODELPARAM_VALUE.MIN_WIDTH PARAM_VALUE.MIN_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.MIN_WIDTH}] ${MODELPARAM_VALUE.MIN_WIDTH}
}

proc update_MODELPARAM_VALUE.TOTAL_THRESHOLD { MODELPARAM_VALUE.TOTAL_THRESHOLD PARAM_VALUE.TOTAL_THRESHOLD } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.TOTAL_THRESHOLD}] ${MODELPARAM_VALUE.TOTAL_THRESHOLD}
}

proc update_MODELPARAM_VALUE.C_S00_AXIS_TDATA_WIDTH { MODELPARAM_VALUE.C_S00_AXIS_TDATA_WIDTH PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_S00_AXIS_TDATA_WIDTH}] ${MODELPARAM_VALUE.C_S00_AXIS_TDATA_WIDTH}
}

proc update_MODELPARAM_VALUE.C_S00_AXI_DATA_WIDTH { MODELPARAM_VALUE.C_S00_AXI_DATA_WIDTH PARAM_VALUE.C_S00_AXI_DATA_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_S00_AXI_DATA_WIDTH}] ${MODELPARAM_VALUE.C_S00_AXI_DATA_WIDTH}
}

proc update_MODELPARAM_VALUE.C_S00_AXI_ADDR_WIDTH { MODELPARAM_VALUE.C_S00_AXI_ADDR_WIDTH PARAM_VALUE.C_S00_AXI_ADDR_WIDTH } {
	# Procedure called to set VHDL generic/Verilog parameter value(s) based on TCL parameter value
	set_property value [get_property value ${PARAM_VALUE.C_S00_AXI_ADDR_WIDTH}] ${MODELPARAM_VALUE.C_S00_AXI_ADDR_WIDTH}
}

