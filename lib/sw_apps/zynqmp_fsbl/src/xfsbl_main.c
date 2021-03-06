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
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*****************************************************************************/
/**
*
* @file xfsbl_main.c
*
* This is the main file which contains code for the FSBL.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date        Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00  kc   10/21/13 Initial release
*
* </pre>
*
* @note
*
******************************************************************************/

/***************************** Include Files *********************************/
#include "xfsbl_hw.h"
#include "xfsbl_main.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

/************************** Variable Definitions *****************************/
XFsblPs FsblInstancePtr;

/*****************************************************************************/
/** This is the FSBL main function and is implemented stage wise.
 *
 * @param	None
 *
 * @return	None
 *
 *****************************************************************************/
int main(void )
{
	/**
	 * Local variables
	 */
	u32 FsblStatus = XFSBL_SUCCESS;
	u32 FsblStage = XFSBL_STAGE1;
	u32 PartitionNum=0U;
	u32 EarlyHandoff = FALSE;

	/**
	 * Initialize globals.
	 */
	FsblInstancePtr.ErrorCode = FsblStatus;

	while (1) {

		switch (FsblStage)
		{

		case XFSBL_STAGE1:
			{
				/**
				 * Initialize the system
				 */
				XFsbl_CfgInitialize(&FsblInstancePtr);

				FsblStatus = XFsbl_Initialize(&FsblInstancePtr);
				if (XFSBL_SUCCESS != FsblStatus)
				{
					FsblStatus += XFSBL_ERROR_STAGE_1;
					FsblStage = XFSBL_STAGE_ERR;
				} else {

					/**
					 *
					 * Include the code for FSBL time measurements
					 * Initialize the global timer and get the value
					 */

					FsblStage = XFSBL_STAGE2;
				}
			}break;

		case XFSBL_STAGE2:
			{

				XFsbl_Printf(DEBUG_INFO,
						"================= In Stage 2 ============ \n\r");

				/**
				 * 	Primary Device
				 *  Secondary boot device
				 *  DeviceOps
				 *  image header
				 *  partition header
				 */
				FsblStatus = XFsbl_BootDeviceInitAndValidate(&FsblInstancePtr);
				if ( (XFSBL_SUCCESS != FsblStatus) &&
						(XFSBL_STATUS_JTAG != FsblStatus) )
				{
					XFsbl_Printf(DEBUG_GENERAL,"Boot Device "
							"Initialization failed 0x%0lx\n\r", FsblStatus);
					FsblStatus += XFSBL_ERROR_STAGE_2;
					FsblStage = XFSBL_STAGE_ERR;
				} else if (XFSBL_STATUS_JTAG == FsblStatus) {
					/**
					 * This is JTAG boot mode, go to the handoff stage
					 */
					FsblStage = XFSBL_STAGE4;
				} else {
					XFsbl_Printf(DEBUG_INFO,"Initialization Success \n\r");

					/**
					 * Start the partition loading from 1
					 * 0th partition will be FSBL
					 */
					PartitionNum = 0x1U;

					FsblStage = XFSBL_STAGE3;
				}
			} break;

		case XFSBL_STAGE3:
			{

				XFsbl_Printf(DEBUG_INFO,
					"======= In Stage 3, Partition No:%d ======= \n\r",
					PartitionNum);

				/**
				 * Load the partitions
				 *  image header
				 *  partition header
				 *  partition parameters
				 */
				FsblStatus = XFsbl_PartitionLoad(&FsblInstancePtr,
								  PartitionNum);
				if (XFSBL_SUCCESS != FsblStatus)
				{
					/**
					 * Error
					 */
					XFsbl_Printf(DEBUG_GENERAL,"Partition %d Load Failed, 0x%0lx\n\r",
							PartitionNum, FsblStatus);
					FsblStatus += XFSBL_ERROR_STAGE_3;
					FsblStage = XFSBL_STAGE_ERR;
				} else {
					XFsbl_Printf(DEBUG_INFO,"Partition %d Load Success \n\r",
									PartitionNum);
					/**
					 * Check loading all partitions is completed
					 */

					FsblStatus = XFsbl_CheckEarlyHandoff(&FsblInstancePtr, PartitionNum);

					if (PartitionNum <
						(FsblInstancePtr.ImageHeader.ImageHeaderTable.NoOfPartitions-1U))
					{
						if (TRUE == FsblStatus) {
							EarlyHandoff = TRUE;
							FsblStage = XFSBL_STAGE4;
						}
						else {
							/**
							 * No need to change the Fsbl Stage
							 * Load the next partition
							 */
							PartitionNum++;
						}
					} else {
						/**
						 * No more partitions present, go to handoff stage
						 */
						XFsbl_Printf(DEBUG_INFO,"All Partitions Loaded \n\r");
						FsblStage = XFSBL_STAGE4;
						EarlyHandoff = FsblStatus;

					}
				} /* End of else loop for Load Success */
			} break;

		case XFSBL_STAGE4:
			{

				XFsbl_Printf(DEBUG_INFO,
						"================= In Stage 4 ============ \n\r");

				/**
				 * Handoff to the applications
				 * Handoff address
				 * xip
				 * ps7 post config
				 */
				FsblStatus = XFsbl_Handoff(&FsblInstancePtr, PartitionNum, EarlyHandoff);

				if (XFSBL_STATUS_CONTINUE_PARTITION_LOAD == FsblStatus) {
					XFsbl_Printf(DEBUG_INFO,"Early handoff to a application complete \n\r");
					XFsbl_Printf(DEBUG_INFO,"Continuing to load remaining partitions \n\r");

					PartitionNum++;
					FsblStage = XFSBL_STAGE3;
				}
				else if (XFSBL_STATUS_CONTINUE_OTHER_HANDOFF == FsblStatus) {
					XFsbl_Printf(DEBUG_INFO,"Early handoff to a application complete \n\r");
					XFsbl_Printf(DEBUG_INFO,"Continuing handoff to other applications, if present \n\r");
					EarlyHandoff = FALSE;
				}
				else if (XFSBL_SUCCESS != FsblStatus) {
					/**
					 * Error
					 */
					XFsbl_Printf(DEBUG_GENERAL,"Handoff Failed 0x%0lx\n\r", FsblStatus);
					FsblStatus += XFSBL_ERROR_STAGE_4;
					FsblStage = XFSBL_STAGE_ERR;
				} else {
					/**
					 * we should never be here
					 */
					FsblStage = XFSBL_STAGE_DEFAULT;
				}
			} break;

		case XFSBL_STAGE_ERR:
			{
				XFsbl_Printf(DEBUG_INFO,
						"================= In Stage Err ============ \n\r");

				XFsbl_ErrorLockDown(FsblStatus);
				/**
				 * we should never be here
				 */
				FsblStage = XFSBL_STAGE_DEFAULT;
			}break;

		case XFSBL_STAGE_DEFAULT:
		default:
			{
				/**
				 * we should never be here
				 */
				XFsbl_Printf(DEBUG_GENERAL,"In default stage: "
						"We should never be here \n\r");

				/**
				 * Exit FSBL
				 */
				XFsbl_HandoffExit(0U, XFSBL_NO_HANDOFFEXIT);

			}break;

		} /* End of switch(FsblStage) */

	} /* End of while(1)  */

	/**
	 * We should never be here
	 */
	XFsbl_Printf(DEBUG_GENERAL,"In default stage: "
				"We should never be here \n\r");
	/**
	 * Exit FSBL
	 */
	XFsbl_HandoffExit(0U, XFSBL_NO_HANDOFFEXIT);

	return 0;
}

void XFsbl_PrintFsblBanner(void )
{
	/**
	 * Print the FSBL Banner
	 */
	XFsbl_Printf(DEBUG_PRINT_ALWAYS,
                 "Xilinx Zynq MP First Stage Boot Loader \n\r");
	XFsbl_Printf(DEBUG_PRINT_ALWAYS,
                 "Release %d.%d   %s  -  %s\r\n",
                 SDK_RELEASE_YEAR, SDK_RELEASE_QUARTER,__DATE__,__TIME__);
	/**
	 * Print the platform
	 */
	if (XFSBL_PLATFORM == XFSBL_PLATFORM_QEMU)
	{
		XFsbl_Printf(DEBUG_GENERAL, "Platform: QEMU, ");
	} else  if (XFSBL_PLATFORM == XFSBL_PLATFORM_REMUS)
	{
		XFsbl_Printf(DEBUG_GENERAL, "Platform: REMUS, ");
	} else  if (XFSBL_PLATFORM == XFSBL_PLATFORM_SILICON)
	{
		XFsbl_Printf(DEBUG_GENERAL, "Platform: Silicon, ");
	} else {
		XFsbl_Printf(DEBUG_GENERAL, "Platform Not identified \r\n");
	}

	return ;
}



/*****************************************************************************/
/**
 * This function is called in FSBL error cases. Error status
 * register is updated and fallback is applied
 *
 * @param ErrorStatus is the error code which is written to the
 * 		  error status register
 *
 * @return none
 *
 * @note Fallback is applied only for fallback supported bootmodes
 *****************************************************************************/
void XFsbl_ErrorLockDown(u32 ErrorStatus)
{
	u32 BootMode=0U;

	/**
	 * Print the FSBL error
	 */
	XFsbl_Printf(DEBUG_GENERAL,"Fsbl Error Status: 0x%08lx\r\n",
		ErrorStatus);

	/**
	 * Update the error status register
	 * and Fsbl instance structure
	 */
	XFsbl_Out32(XFSBL_ERROR_STATUS_REGISTER_OFFSET, ErrorStatus);
	FsblInstancePtr.ErrorCode = ErrorStatus;

	/**
	 * Read Boot Mode register
	 */
	BootMode = XFsbl_In32(CRL_APB_BOOT_MODE_USER) &
			CRL_APB_BOOT_MODE_USER_BOOT_MODE_MASK;

	/**
	 * Fallback if bootmode supports
	 */
	if ( (BootMode == XFSBL_QSPI24_BOOT_MODE) ||
			(BootMode == XFSBL_QSPI32_BOOT_MODE) ||
			(BootMode == XFSBL_NAND_BOOT_MODE) ||
			(BootMode == XFSBL_SD0_BOOT_MODE) ||
			(BootMode == XFSBL_EMMC_BOOT_MODE) ||
			(BootMode == XFSBL_SD1_BOOT_MODE) ||
			(BootMode == XFSBL_SD1_LS_BOOT_MODE) )
	{
		XFsbl_FallBack();
	} else {
		/**
		 * Be in while loop if fallback is not supported
		 */
		XFsbl_Printf(DEBUG_GENERAL,"Fallback not supported \n\r");

		/**
		 * Exit FSBL
		 */
		XFsbl_HandoffExit(0U, XFSBL_NO_HANDOFFEXIT);
	}

	/**
	 * Should never be here
	 */
	return ;
}

/*****************************************************************************/
/**
 * In Fallback,  soft reset is applied to the system after incrementing
 * the multiboot register. A hook is provided to before the fallback so
 * that users can write their own code before soft reset
 *
 * @param none
 *
 * @return none
 *
 * @note We will not return from this function as it does soft reset
 *****************************************************************************/
void XFsbl_FallBack(void)
{
	u32 RegValue;

#ifdef XFSBL_WDT_PRESENT
	/* Stop WDT as we are restarting */
	XFsbl_StopWdt();
#endif

	/* Hook before FSBL Fallback */
	XFsbl_HookBeforeFallback();

	/* Read the Multiboot register */
	RegValue = XFsbl_In32(CSU_CSU_MULTI_BOOT);

	XFsbl_Printf(DEBUG_GENERAL,"Performing FSBL FallBack\n\r");

	XFsbl_UpdateMultiBoot(RegValue+1);

	return;
}


/*****************************************************************************/
/**
 * This is the function which actually updates the multiboot register and
 * does the soft reset. This function is called in fallback case and
 * in the cases where user would like to jump to a different image,
 * corresponding to the multiboot value being passed to this function.
 * The latter case is a generic one and need arise because of error scenario.
 *
 * @param MultiBootValue is the new value for the multiboot register
 *
 * @return none
 *
 * @note We will not return from this function as it does soft reset
 *****************************************************************************/

void XFsbl_UpdateMultiBoot(u32 MultiBootValue)
{
	u32 RegValue;

	XFsbl_Out32(CSU_CSU_MULTI_BOOT, MultiBootValue);

	/* make sure every thing completes */
	dsb();
	isb();

	/* Soft reset the system */
	XFsbl_Printf(DEBUG_GENERAL,"Performing System Soft Reset\n\r");
	RegValue = XFsbl_In32(CRL_APB_RESET_CTRL);
	XFsbl_Out32(CRL_APB_RESET_CTRL,
			RegValue|CRL_APB_RESET_CTRL_SOFT_RESET_MASK);

	/* wait here until reset happens */
	while(1);

	return;

}
