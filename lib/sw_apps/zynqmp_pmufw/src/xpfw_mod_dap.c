/******************************************************************************
*
* Copyright (C) 2015 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* XILINX CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

#include "xpfw_default.h"
#include "xpfw_rom_interface.h"
#include "xpfw_config.h"
#include "xpfw_core.h"
#include "xpfw_events.h"
#include "xpfw_module.h"

/* Enable DAP wake mod only if PM is disabled, to avoid conflicts */
#ifndef ENABLE_PM
/* CfgInit Handler */
static void DapCfgInit(const XPfw_Module_t *ModPtr, const u32 *CfgData, u32 Len)
{
	/* Used for DAP Wakes */
	XPfw_CoreRegisterEvent(ModPtr, XPFW_EV_DAP_RPU_WAKE);
	XPfw_CoreRegisterEvent(ModPtr, XPFW_EV_DAP_FPD_WAKE);

	fw_printf("DAP_WAKE (MOD-%d): Initialized.\r\n", ModPtr->ModId);
}

/* Event Handler */
static void DapEventHandler(const XPfw_Module_t *ModPtr, u32 EventId)
{
	if (XPFW_EV_DAP_RPU_WAKE == EventId) {
		/* Call ROM Handler for RPU Wake */
		XpbrServHndlrTbl[XPBR_SERV_EXT_DAPRPUWAKE]();
		fw_printf("XPFW: DAP RPU WAKE.. Done\r\n");
	}
	if (XPFW_EV_DAP_FPD_WAKE == EventId) {
		/* Call ROM Handler for FPD Wake */
		XpbrServHndlrTbl[XPBR_SERV_EXT_DAPFPDWAKE]();
		fw_printf("XPFW: DAP FPD WAKE.. Done\r\n");
	}
}

/*
 * Create a Mod and assign the Handlers. We will call this function
 * from XPfw_UserStartup()
 */
void ModDapInit(void)
{
	const XPfw_Module_t *DapModPtr = XPfw_CoreCreateMod();

	(void) XPfw_CoreSetCfgHandler(DapModPtr, DapCfgInit);
	(void) XPfw_CoreSetEventHandler(DapModPtr, DapEventHandler);
}
#else
	void ModDapInit(void) { }
#endif
