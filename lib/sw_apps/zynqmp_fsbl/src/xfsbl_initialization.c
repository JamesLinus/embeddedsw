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
* @file xfsbl_initilization.c
*
* This is the file which contains initialization code for the FSBL.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date        Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00  kc   10/21/13 Initial release
* 2.00  sg   13/03/15 Added QSPI 32Bit bootmode
*
* </pre>
*
* @note
*
******************************************************************************/

/***************************** Include Files *********************************/
#include "xfsbl_hw.h"
#include "xfsbl_main.h"
#include "xfsbl_misc_drivers.h"
#include "psu_init.h"
#include "xfsbl_qspi.h"
#include "xfsbl_csu_dma.h"

/************************** Constant Definitions *****************************/
#define XFSBL_R5_VECTOR_VALUE 	0xEAFEFFFEU

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/
static u32 XFsbl_ProcessorInit(XFsblPs * FsblInstancePtr);
static u32 XFsbl_ResetValidation(XFsblPs * FsblInstancePtr);
static u32 XFsbl_SystemInit(XFsblPs * FsblInstancePtr);
static u32 XFsbl_PrimaryBootDeviceInit(XFsblPs * FsblInstancePtr);
static u32 XFsbl_ValidateHeader(XFsblPs * FsblInstancePtr);
static u32 XFsbl_SecondaryBootDeviceInit(XFsblPs * FsblInstancePtr);

/* Functions from xfsbl_misc.c */
int psu_init();

/**
 * Functions from xfsbl_misc.c
 */
void XFsbl_RegisterHandlers(void);

/**
 *Functions from xfsbl_qspi.c
 */



/************************** Variable Definitions *****************************/


/****************************************************************************/
/**
 * This function is used to initialize the FsblInstance with the
 * default values
 *
 * @param  FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return None
 *
 * @note
 *
 *****************************************************************************/
void XFsbl_CfgInitialize (XFsblPs * FsblInstancePtr)
{
	FsblInstancePtr->Version = 0x3U;
	FsblInstancePtr->ErrorCode = XFSBL_SUCCESS;
	FsblInstancePtr->HandoffCpuNo = 0U;
}

/*****************************************************************************/
/**
 * This function is initializes the processor and system.
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return
 *          - returns the error codes described in xfsbl_error.h on any error
 * 			- returns XFSBL_SUCCESS on success
 *
 *****************************************************************************/
u32 XFsbl_Initialize(XFsblPs * FsblInstancePtr)
{
	u32 Status = XFSBL_SUCCESS;

	/**
	 * Configure the system as in PSU
	 */
	Status = XFsbl_SystemInit(FsblInstancePtr);
	if (XFSBL_SUCCESS != Status) {
		goto END;
	}

	/**
	 * Print the FSBL banner
	 */
	XFsbl_PrintFsblBanner();

	/**
	 * Initialize the processor
	 */
	Status = XFsbl_ProcessorInit(FsblInstancePtr);
	if (XFSBL_SUCCESS != Status) {
		goto END;
	}

	/**
	 * Validate the reset reason
	 */
	Status = XFsbl_ResetValidation(FsblInstancePtr);
	if (XFSBL_SUCCESS != Status) {
		goto END;
	}

	XFsbl_Printf(DEBUG_INFO,"Processor Initialization Done \n\r");
END:
	return Status;
}


/*****************************************************************************/
/**
 * This function initializes the primary and secondary boot devices
 * and validates the image header
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 ******************************************************************************/
u32 XFsbl_BootDeviceInitAndValidate(XFsblPs * FsblInstancePtr)
{
	u32 Status = XFSBL_SUCCESS;

	/**
	 * Configure the primary boot device
	 */
	Status = XFsbl_PrimaryBootDeviceInit(FsblInstancePtr);
	if (XFSBL_SUCCESS != Status) {
		goto END;
	}

	/**
	 * Read and Validate the header
	 */
	Status = XFsbl_ValidateHeader(FsblInstancePtr);
	if (XFSBL_SUCCESS != Status) {
		goto END;
	}

	/**
	 * Update the secondary boot device
	 */
	FsblInstancePtr->SecondaryBootDevice =
	 FsblInstancePtr->ImageHeader.ImageHeaderTable.PartitionPresentDevice;

	/**
	 *  Configure the secondary boot device if required
	 */
	if (FsblInstancePtr->SecondaryBootDevice !=
			FsblInstancePtr->PrimaryBootDevice) {
		Status = XFsbl_SecondaryBootDeviceInit(FsblInstancePtr);
		if (XFSBL_SUCCESS != Status) {
			goto END;
		}
	}

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function initializes the processor and updates the cluster id
 * which indicates CPU on which fsbl is running
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_ProcessorInit(XFsblPs * FsblInstancePtr)
{
	u32 Status = XFSBL_SUCCESS;
	//u64 ClusterId=0U;
	PTRSIZE ClusterId=0U;
	u32 RegValue;
	u32 Index=0U;

	/**
	 * Read the cluster ID and Update the Processor ID
	 * Initialize the processor settings that are not done in
	 * BSP startup code
	 */
#ifdef XFSBL_A53
	ClusterId = mfcp(MPIDR_EL1);
#else
	ClusterId = mfcp(XREG_CP15_MULTI_PROC_AFFINITY);
#endif

	XFsbl_Printf(DEBUG_INFO,"Cluster ID 0x%0lx\n\r", ClusterId);

	if (XFSBL_PLATFORM == XFSBL_PLATFORM_QEMU) {
		/**
		 * Remmaping for R5 in QEMU
		 */
		if (ClusterId == 0x80000004U) {
			ClusterId = 0xC0000100U;
		} else if (ClusterId == 0x80000005U) {
			/* this corresponds to R5-1 */
			Status = XFSBL_ERROR_UNSUPPORTED_CLUSTER_ID;
			XFsbl_Printf(DEBUG_GENERAL,
					"XFSBL_ERROR_UNSUPPORTED_CLUSTER_ID\n\r");
			goto END;
		} else {
			/* For MISRA C compliance */
		}
	}

	/* store the processor ID based on the cluster ID */
	if ((ClusterId & XFSBL_CLUSTER_ID_MASK) == XFSBL_A53_PROCESSOR) {
		XFsbl_Printf(DEBUG_GENERAL,"Running on A53-0 ");
		FsblInstancePtr->ProcessorID =
				XIH_PH_ATTRB_DEST_CPU_A53_0;
#ifdef __aarch64__
		/* Running on A53 64-bit */
		XFsbl_Printf(DEBUG_GENERAL,"(64-bit) Processor \n\r");
		FsblInstancePtr->A53ExecState = XIH_PH_ATTRB_A53_EXEC_ST_AA64;
#else
		/* Running on A53 32-bit */
		XFsbl_Printf(DEBUG_GENERAL,"(32-bit) Processor \n\r");
		FsblInstancePtr->A53ExecState = XIH_PH_ATTRB_A53_EXEC_ST_AA32;
#endif

	} else if ((ClusterId & XFSBL_CLUSTER_ID_MASK) == XFSBL_R5_PROCESSOR) {
		/* A53ExecState is not valid for R5 */
		FsblInstancePtr->A53ExecState = XIH_INVALID_EXEC_ST;

		RegValue = XFsbl_In32(RPU_RPU_GLBL_CNTL);
		if ((RegValue & RPU_RPU_GLBL_CNTL_SLSPLIT_MASK) == 0U) {
			XFsbl_Printf(DEBUG_GENERAL,
				"Running on R5 Processor in Lockstep \n\r");
			FsblInstancePtr->ProcessorID =
				XIH_PH_ATTRB_DEST_CPU_R5_L;
		} else {
			XFsbl_Printf(DEBUG_GENERAL,
				"Running on R5-0 Processor \n\r");
			FsblInstancePtr->ProcessorID =
				XIH_PH_ATTRB_DEST_CPU_R5_0;
		}

		/**
		 * Update the Vector locations in R5 TCM
		 */
		while (Index<32U) {
			XFsbl_Out32(Index, 0U);
			XFsbl_Out32(Index, XFSBL_R5_VECTOR_VALUE);
			Index += 4;
		}

	} else {
		Status = XFSBL_ERROR_UNSUPPORTED_CLUSTER_ID;
		XFsbl_Printf(DEBUG_GENERAL,
				"XFSBL_ERROR_UNSUPPORTED_CLUSTER_ID\n\r");
		goto END;
	}

	/**
	 * Register the exception handlers
	 */
	XFsbl_RegisterHandlers();

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function validates the reset reason
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/

static u32 XFsbl_ResetValidation(XFsblPs * FsblInstancePtr)
{
	u32 Status =  XFSBL_SUCCESS;
	u32 FsblErrorStatus=0U;
	u32 ResetReasonValue=0U;
	u32 ErrStatusRegValue;

	/**
	 *  Read the Error Status register
	 *  If WDT reset, do fallback
	 */
	FsblErrorStatus = XFsbl_In32(XFSBL_ERROR_STATUS_REGISTER_OFFSET);

	ResetReasonValue = XFsbl_In32(CRL_APB_RESET_REASON);
	ErrStatusRegValue = XFsbl_In32(PMU_GLOBAL_ERROR_STATUS_1);

	/**
	 * Check if the reset is due to system WDT during
	 * previous FSBL execution
	 */
	if (((ResetReasonValue & CRL_APB_RESET_REASON_PMU_SYS_RESET_MASK)
			== CRL_APB_RESET_REASON_PMU_SYS_RESET_MASK) &&
			((ErrStatusRegValue & PMU_GLOBAL_ERROR_STATUS_1_LPD_SWDT_MASK)
			== PMU_GLOBAL_ERROR_STATUS_1_LPD_SWDT_MASK) &&
			(FsblErrorStatus == XFSBL_RUNNING)) {
		/**
		 * reset is due to System WDT.
		 * Do a fallback
		 */
		Status = XFSBL_ERROR_SYSTEM_WDT_RESET;
		XFsbl_Printf(DEBUG_GENERAL,"XFSBL_ERROR_SYSTEM_WDT_RESET\n\r");
		goto END;
	}

	/**
	 * Mark FSBL running in error status register to
	 * detect the WDT reset while FSBL execution
	 */
	if (FsblErrorStatus != XFSBL_RUNNING) {
		XFsbl_Out32(XFSBL_ERROR_STATUS_REGISTER_OFFSET,
						  XFSBL_RUNNING);
	}

	/**
	 *  Read system error status register
	 * 	provide FsblHook function for any action
	 */

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function initializes the system using the psu_init()
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_SystemInit(XFsblPs * FsblInstancePtr)
{
	u32 Status =  XFSBL_SUCCESS;

	/**
	 * psu initialization
	 */
    Status = (u32)psu_init();
	if (XFSBL_SUCCESS != Status) {
		XFsbl_Printf(DEBUG_GENERAL,"XFSBL_PSU_INIT_FAILED\n\r");
		/**
		 * Need to check a way to communicate both FSBL code
		 * and PSU init error code
		 */
		Status = XFSBL_PSU_INIT_FAILED + Status;
		goto END;
	}

	/**
	 * DDR Check if present
	 */


	/**
	 * Poweroff the unused blocks as per PSU
	 */

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function initializes the primary boot device
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_PrimaryBootDeviceInit(XFsblPs * FsblInstancePtr)
{
	u32 Status =  XFSBL_SUCCESS;
	u32 BootMode=0U;

	/**
	 * Read Boot Mode register and update the value
	 */
	BootMode = XFsbl_In32(CRL_APB_BOOT_MODE_USER) &
			CRL_APB_BOOT_MODE_USER_BOOT_MODE_MASK;

	FsblInstancePtr->PrimaryBootDevice = BootMode;

	/**
	 * Enable drivers only if they are device boot modes
	 * Not required for JTAG modes
	 */
	if ( (BootMode == XFSBL_QSPI24_BOOT_MODE) ||
			(BootMode == XFSBL_QSPI32_BOOT_MODE) ||
			(BootMode == XFSBL_NAND_BOOT_MODE) ||
			(BootMode == XFSBL_SD0_BOOT_MODE) ||
			(BootMode == XFSBL_EMMC_BOOT_MODE) ||
			(BootMode == XFSBL_SD1_BOOT_MODE) ||
			(BootMode == XFSBL_SD1_LS_BOOT_MODE)) {
		/**
		 * Initialize the WDT and CSU drivers
		 */
#ifdef XFSBL_WDT_PRESENT
		Status = XFsbl_InitWdt();
		if (XFSBL_SUCCESS != Status) {
			XFsbl_Printf(DEBUG_GENERAL,"WDT initialization failed \n\r");
			goto END;
		}
#endif

		/* Initialize CSUDMA driver */
		Status = XFsbl_CsuDmaInit();
		if (XFSBL_SUCCESS != Status) {
			goto END;
		}
	}

	switch(BootMode)
	{
		/**
		 * For JTAG boot mode, it will be in while loop
		 */
		case XFSBL_JTAG_BOOT_MODE:
		{
			XFsbl_Printf(DEBUG_GENERAL,"In JTAG Boot Mode \n\r");
			Status = XFSBL_STATUS_JTAG;
		}
		break;

		case XFSBL_QSPI24_BOOT_MODE:
		{
			XFsbl_Printf(DEBUG_GENERAL,"QSPI 24bit Boot Mode \n\r");
#ifdef XFSBL_QSPI
			/**
			 * Update the deviceops structure with necessary values
			 */
			FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_Qspi24Init;
			FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_Qspi24Copy;
			FsblInstancePtr->DeviceOps.DeviceRelease = XFsbl_Qspi24Release;
#else
			/**
			 * This bootmode is not supported in this release
			 */
			XFsbl_Printf(DEBUG_GENERAL,
				"XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
			Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
#endif
		}
		break;

		case XFSBL_QSPI32_BOOT_MODE:
		{
			XFsbl_Printf(DEBUG_GENERAL,"QSPI 32 bit Boot Mode \n\r");
#ifdef XFSBL_QSPI
			/**
			 * Update the deviceops structure with necessary values
			 *
			 */
            FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_Qspi32Init;
			FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_Qspi32Copy;
			FsblInstancePtr->DeviceOps.DeviceRelease = XFsbl_Qspi32Release;
#else
			/**
			 * This bootmode is not supported in this release
			 */
			XFsbl_Printf(DEBUG_GENERAL,
					"XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
			Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
#endif
        }
        break;

		case XFSBL_NAND_BOOT_MODE:
		{
			XFsbl_Printf(DEBUG_GENERAL,"NAND Boot Mode \n\r");
#ifdef XFSBL_NAND
			/**
			 * Update the deviceops structure with necessary values
			 *
			 */
			FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_NandInit;
			FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_NandCopy;
			FsblInstancePtr->DeviceOps.DeviceRelease =
							XFsbl_NandRelease;
#else
			/**
			 * This bootmode is not supported in this release
			 */
			XFsbl_Printf(DEBUG_GENERAL,
				"XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
			Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
#endif
		} break;

		case XFSBL_SD0_BOOT_MODE:
		case XFSBL_EMMC_BOOT_MODE:
		{
			if (BootMode == XFSBL_SD0_BOOT_MODE) {
				XFsbl_Printf(DEBUG_GENERAL,"SD0 Boot Mode \n\r");
			}
			else {
				XFsbl_Printf(DEBUG_GENERAL,"eMMC Boot Mode \n\r");
			}
#ifdef XFSBL_SD_0
			/**
			 * Update the deviceops structure with necessary values
			 */
			FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_SdInit;
			FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_SdCopy;
			FsblInstancePtr->DeviceOps.DeviceRelease = XFsbl_SdRelease;
#else
			/**
			 * This bootmode is not supported in this release
			 */
			XFsbl_Printf(DEBUG_GENERAL,
				"XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
			Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
#endif
		} break;

		case XFSBL_SD1_BOOT_MODE:
		case XFSBL_SD1_LS_BOOT_MODE:
		{
			if (BootMode == XFSBL_SD1_BOOT_MODE) {
				XFsbl_Printf(DEBUG_GENERAL, "SD1 Boot Mode \n\r");
			}
			else {
				XFsbl_Printf(DEBUG_GENERAL,
						"SD1 with level shifter Boot Mode \n\r");
			}
#ifdef XFSBL_SD_1
			/**
			 * Update the deviceops structure with necessary values
			 */
			FsblInstancePtr->DeviceOps.DeviceInit = XFsbl_SdInit;
			FsblInstancePtr->DeviceOps.DeviceCopy = XFsbl_SdCopy;
			FsblInstancePtr->DeviceOps.DeviceRelease = XFsbl_SdRelease;
#else
			/**
			 * This bootmode is not supported in this release
			 */
			XFsbl_Printf(DEBUG_GENERAL,
				"XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
			Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
#endif
		} break;

		default:
		{
			XFsbl_Printf(DEBUG_GENERAL,
					"XFSBL_ERROR_UNSUPPORTED_BOOT_MODE\n\r");
			Status = XFSBL_ERROR_UNSUPPORTED_BOOT_MODE;
		} break;

	}

	/**
	 * In case of error or Jtag boot, goto end
	 */
	if (XFSBL_SUCCESS != Status) {
		goto END;
	}

	/**
	 * Initialize the Device Driver
	 */
	Status = FsblInstancePtr->DeviceOps.DeviceInit(BootMode);
	if (XFSBL_SUCCESS != Status) {
		goto END;
	}

END:
	return Status;
}

/*****************************************************************************/
/**
 * This function validates the image header
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_ValidateHeader(XFsblPs * FsblInstancePtr)
{
	u32 Status =  XFSBL_SUCCESS;
	u32 MultiBootOffset=0U;
	u32 BootHdrAttrb=0U;
	u32 FlashImageOffsetAddress=0U;
	u32 EfuseCtrl=0U;

	/**
	 * Read the Multiboot Register
	 */
	MultiBootOffset = XFsbl_In32(CSU_CSU_MULTI_BOOT);
	XFsbl_Printf(DEBUG_INFO,"Multiboot Reg : 0x%0lx \n\r", MultiBootOffset);

	/**
	 *  Calculate the Flash Offset Address
	 *  For file system based devices, Flash Offset Address should be 0 always
	 */
	if ((FsblInstancePtr->PrimaryBootDevice == XFSBL_SD0_BOOT_MODE) ||
			(FsblInstancePtr->PrimaryBootDevice == XFSBL_EMMC_BOOT_MODE) ||
			(FsblInstancePtr->PrimaryBootDevice == XFSBL_SD1_BOOT_MODE) ||
			(FsblInstancePtr->PrimaryBootDevice == XFSBL_SD1_LS_BOOT_MODE))
	{
		FsblInstancePtr->ImageOffsetAddress = 0x0U;
	} else {
		FsblInstancePtr->ImageOffsetAddress =
				MultiBootOffset * XFSBL_IMAGE_SEARCH_OFFSET;
	}

	FlashImageOffsetAddress = FsblInstancePtr->ImageOffsetAddress;

	/**
	 * Read Boot Image attributes
	 */
	Status = FsblInstancePtr->DeviceOps.DeviceCopy(FlashImageOffsetAddress
                    + XIH_BH_IMAGE_ATTRB_OFFSET,
                   (PTRSIZE ) &BootHdrAttrb, XIH_FIELD_LEN);
        if (XFSBL_SUCCESS != Status) {
                XFsbl_Printf(DEBUG_GENERAL,"Device Copy Failed \n\r");
                goto END;
        }
	FsblInstancePtr->BootHdrAttributes = BootHdrAttrb;

	/**
	 * Read Image Header and validate Image Header Table
	 */
	Status = XFsbl_ReadImageHeader(&FsblInstancePtr->ImageHeader,
					&FsblInstancePtr->DeviceOps,
					FlashImageOffsetAddress,
					FsblInstancePtr->ProcessorID);
	if (XFSBL_SUCCESS != Status) {
		goto END;
	}


	/**
	 * Read Efuse bit and check Boot Header for Authentication
	 */
	EfuseCtrl = XFsbl_In32(EFUSE_SEC_CTRL);
	if (((EfuseCtrl & EFUSE_SEC_CTRL_RSA_EN_MASK) != 0) ||
	    ((BootHdrAttrb & XIH_BH_IMAGE_ATTRB_RSA_MASK)
		== XIH_BH_IMAGE_ATTRB_RSA_MASK)) {

		XFsbl_Printf(DEBUG_INFO,"Authentication Enabled\r\n");
#ifdef XFSBL_RSA
		/**
		 * Authenticate the image header
		 */

#else
                XFsbl_Printf(DEBUG_GENERAL,"Rsa code not Enabled\r\n");
                Status = XFSBL_ERROR_RSA_NOT_ENABLED;
                goto END;
#endif
	}
END:
	return Status;
}

/*****************************************************************************/
/**
 * This function initializes secondary boot device
 *
 * @param	FsblInstancePtr is pointer to the XFsbl Instance
 *
 * @return	returns the error codes described in xfsbl_error.h on any error
 * 			returns XFSBL_SUCCESS on success
 *
 ******************************************************************************/
static u32 XFsbl_SecondaryBootDeviceInit(XFsblPs * FsblInstancePtr)
{
	u32 Status = XFSBL_SUCCESS;

	/**
	 * Update the deviceops structure
	 */


	/**
	 * Initialize the Secondary Boot Device Driver
	 */

	return Status;
}
