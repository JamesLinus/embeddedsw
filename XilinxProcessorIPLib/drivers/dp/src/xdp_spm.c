/*******************************************************************************
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
*******************************************************************************/
/******************************************************************************/
/**
 *
 * @file xdp_spm.c
 * @addtogroup dp_v3_0
 * @{
 *
 * This file contains the stream policy maker functions for the XDp driver.
 * These functions set up the DisplayPort TX core's main stream attributes (MSA)
 * that determine how a video stream will be displayed and also some DisplayPort
 * RX MSA-related functions.
 *
 * @note	None.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- -----------------------------------------------
 * 1.0   als  01/20/15 Initial release. TX code merged from the dptx driver.
 * 2.0   als  07/27/15 Scale TX fractional register by 1024 instead of 1000.
 * 3.0   als  10/07/15 Added MSA callback.
 * </pre>
 *
*******************************************************************************/

/******************************* Include Files ********************************/

#include "xdp.h"

/**************************** Function Prototypes *****************************/

static void XDp_TxCalculateTs(XDp *InstancePtr, u8 Stream, u8 BitsPerPixel);

/**************************** Function Definitions ****************************/

/******************************************************************************/
/**
 * This function calculates the following Main Stream Attributes (MSA):
 *	- Transfer unit size
 *	- User pixel width
 *	- Horizontal start
 *	- Vertical start
 *	- Horizontal total clock
 *	- Vertical total clock
 *	- Misc0
 *	- Misc1
 *	- Data per lane
 *	- Average number of bytes per transfer unit
 *	- Number of initial wait cycles
 * These values are derived from:
 *	- Bits per color
 *	- Horizontal resolution
 *	- Vertical resolution
 *	- Pixel clock (in KHz)
 *	- Horizontal sync polarity
 *	- Vertical sync polarity
 *	- Horizontal front porch
 *	- Horizontal sync pulse width
 *	- Horizontal back porch
 *	- Vertical front porch
 *	- Vertical sync pulse width
 *	- Vertical back porch
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which to calculate the MSA
 *		values.
 *
 * @return	None.
 *
 * @note	The MsaConfig structure is modified with the new, calculated
 *		values. The main stream attributes that were used to derive the
 *		calculated values are untouched in the MsaConfig structure.
 *
*******************************************************************************/
void XDp_TxCfgMsaRecalculate(XDp *InstancePtr, u8 Stream)
{
	u32 VideoBw;
	u32 LinkBw;
	u32 WordsPerLine;
	u8 BitsPerPixel;
	XDp_TxMainStreamAttributes *MsaConfig;
	XDp_TxLinkConfig *LinkConfig;

	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_TX);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
		(Stream == XDP_TX_STREAM_ID2) || (Stream == XDP_TX_STREAM_ID3) ||
		(Stream == XDP_TX_STREAM_ID4));

	MsaConfig = &InstancePtr->TxInstance.MsaConfig[Stream - 1];
	LinkConfig = &InstancePtr->TxInstance.LinkConfig;

	/* Verify the rest of the values used. */
	Xil_AssertVoid((LinkConfig->LinkRate == XDP_TX_LINK_BW_SET_162GBPS) ||
			(LinkConfig->LinkRate == XDP_TX_LINK_BW_SET_270GBPS) ||
			(LinkConfig->LinkRate == XDP_TX_LINK_BW_SET_540GBPS));
	Xil_AssertVoid((LinkConfig->LaneCount == XDP_TX_LANE_COUNT_SET_1) ||
			(LinkConfig->LaneCount == XDP_TX_LANE_COUNT_SET_2) ||
			(LinkConfig->LaneCount == XDP_TX_LANE_COUNT_SET_4));
	Xil_AssertVoid((MsaConfig->SynchronousClockMode == 0) ||
				(MsaConfig->SynchronousClockMode == 1));
	Xil_AssertVoid((MsaConfig->DynamicRange == 0) ||
					(MsaConfig->DynamicRange == 1));
	Xil_AssertVoid((MsaConfig->YCbCrColorimetry == 0) ||
					(MsaConfig->YCbCrColorimetry == 1));
	Xil_AssertVoid((MsaConfig->BitsPerColor == 6) ||
					(MsaConfig->BitsPerColor == 8) ||
					(MsaConfig->BitsPerColor == 10) ||
					(MsaConfig->BitsPerColor == 12) ||
					(MsaConfig->BitsPerColor == 16));

	/* Set the user pixel width to handle clocks that exceed the
	 * capabilities of the DisplayPort TX core. */
	if (MsaConfig->OverrideUserPixelWidth == 0) {
		if ((MsaConfig->PixelClockHz > 300000000) &&
			(LinkConfig->LaneCount == XDP_TX_LANE_COUNT_SET_4)) {
			MsaConfig->UserPixelWidth = 4;
		}
		else if ((MsaConfig->PixelClockHz > 75000000) &&
			(LinkConfig->LaneCount != XDP_TX_LANE_COUNT_SET_1)) {
			MsaConfig->UserPixelWidth = 2;
		}
		else {
			MsaConfig->UserPixelWidth = 1;
		}
	}

	/* Compute the rest of the MSA values. */
	MsaConfig->NVid = 27 * 1000 * LinkConfig->LinkRate;
	MsaConfig->HStart = MsaConfig->Vtm.Timing.HSyncWidth +
					MsaConfig->Vtm.Timing.HBackPorch;
	MsaConfig->VStart = MsaConfig->Vtm.Timing.F0PVSyncWidth +
					MsaConfig->Vtm.Timing.F0PVBackPorch;

	/* Miscellaneous attributes. */
	if (MsaConfig->BitsPerColor == 6) {
		MsaConfig->Misc0 = XDP_TX_MAIN_STREAMX_MISC0_BDC_6BPC;
	}
	else if (MsaConfig->BitsPerColor == 8) {
		MsaConfig->Misc0 = XDP_TX_MAIN_STREAMX_MISC0_BDC_8BPC;
	}
	else if (MsaConfig->BitsPerColor == 10) {
		MsaConfig->Misc0 = XDP_TX_MAIN_STREAMX_MISC0_BDC_10BPC;
	}
	else if (MsaConfig->BitsPerColor == 12) {
		MsaConfig->Misc0 = XDP_TX_MAIN_STREAMX_MISC0_BDC_12BPC;
	}
	else if (MsaConfig->BitsPerColor == 16) {
		MsaConfig->Misc0 = XDP_TX_MAIN_STREAMX_MISC0_BDC_16BPC;
	}
	MsaConfig->Misc0 = (MsaConfig->Misc0 <<
			XDP_TX_MAIN_STREAMX_MISC0_BDC_SHIFT) |
			(MsaConfig->YCbCrColorimetry <<
			XDP_TX_MAIN_STREAMX_MISC0_YCBCR_COLORIMETRY_SHIFT) |
			(MsaConfig->DynamicRange <<
			XDP_TX_MAIN_STREAMX_MISC0_DYNAMIC_RANGE_SHIFT) |
			(MsaConfig->ComponentFormat <<
			XDP_TX_MAIN_STREAMX_MISC0_COMPONENT_FORMAT_SHIFT) |
			(MsaConfig->SynchronousClockMode);
	MsaConfig->Misc1 = 0;

	/* Determine the number of bits per pixel for the specified color
	 * component format. */
	if (MsaConfig->ComponentFormat ==
			XDP_TX_MAIN_STREAMX_MISC0_COMPONENT_FORMAT_YCBCR422) {
		/* YCbCr422 color component format. */
		BitsPerPixel = MsaConfig->BitsPerColor * 2;
	}
	else {
		/* RGB or YCbCr 4:4:4 color component format. */
		BitsPerPixel = MsaConfig->BitsPerColor * 3;
	}

	/* Calculate the data per lane. */
	WordsPerLine = (MsaConfig->Vtm.Timing.HActive * BitsPerPixel);
	if ((WordsPerLine % 16) != 0) {
		WordsPerLine += 16;
	}
	WordsPerLine /= 16;

	MsaConfig->DataPerLane = WordsPerLine - LinkConfig->LaneCount;
	if ((WordsPerLine % LinkConfig->LaneCount) != 0) {
		MsaConfig->DataPerLane +=
					(WordsPerLine % LinkConfig->LaneCount);
	}

	if (InstancePtr->TxInstance.MstEnable == 1) {
		/* Do time slot (and payload bandwidth number) calculations for
		 * MST. */
		XDp_TxCalculateTs(InstancePtr, Stream, BitsPerPixel);

		MsaConfig->InitWait = 0;
	}
	else {
		/* Allocate a fixed size for single-stream transport (SST)
		 * operation. */
		MsaConfig->TransferUnitSize = 64;

		/* Calculate the average number of bytes per transfer unit.
		 * Note: Both the integer and the fractional part is stored in
		 * AvgBytesPerTU. */
		VideoBw = ((MsaConfig->PixelClockHz / 1000) * BitsPerPixel) / 8;
		LinkBw = (LinkConfig->LaneCount * LinkConfig->LinkRate * 27);
		MsaConfig->AvgBytesPerTU = (VideoBw *
					MsaConfig->TransferUnitSize) / LinkBw;

		/* The number of initial wait cycles at the start of a new line
		 * by the framing logic. This allows enough data to be buffered
		 * in the input FIFO before video is sent. */
		if ((MsaConfig->AvgBytesPerTU / 1000) <= 4) {
			MsaConfig->InitWait = 64;
		}
		else {
			MsaConfig->InitWait = MsaConfig->TransferUnitSize -
					(MsaConfig->AvgBytesPerTU / 1000);
		}
	}
}

/******************************************************************************/
/**
 * This function sets the Main Stream Attribute (MSA) values in the
 * configuration structure to match one of the standard display mode timings
 * from the XDp_TxDmtModes[] standard Display Monitor Timing (DMT) table. The
 * XDp_TxVideoMode enumeration in xvidc.h lists the available video modes.
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which the MSA values will be
 *		used for.
 * @param	VideoMode is one of the enumerated standard video modes that is
 *		used to determine the MSA values to be used.
 *
 * @return	None.
 *
 * @note	The InstancePtr->TxInstance.MsaConfig structure is modified to
 *		reflect the MSA values associated to the specified video mode.
 *
*******************************************************************************/
void XDp_TxCfgMsaUseStandardVideoMode(XDp *InstancePtr, u8 Stream,
						XVidC_VideoMode VideoMode)
{
	XDp_TxMainStreamAttributes *MsaConfig;

	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_TX);
	Xil_AssertVoid(VideoMode < XVIDC_VM_NUM_SUPPORTED);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
		(Stream == XDP_TX_STREAM_ID2) || (Stream == XDP_TX_STREAM_ID3) ||
		(Stream == XDP_TX_STREAM_ID4));

	MsaConfig = &InstancePtr->TxInstance.MsaConfig[Stream - 1];

	/* Configure the MSA values from the display monitor DMT table. */
	MsaConfig->Vtm.VmId = XVidC_VideoTimingModes[VideoMode].VmId;
	MsaConfig->Vtm.FrameRate = XVidC_VideoTimingModes[VideoMode].FrameRate;
	MsaConfig->Vtm.Timing.HActive =
			XVidC_VideoTimingModes[VideoMode].Timing.HActive;
	MsaConfig->Vtm.Timing.HFrontPorch =
			XVidC_VideoTimingModes[VideoMode].Timing.HFrontPorch;
	MsaConfig->Vtm.Timing.HSyncWidth =
			XVidC_VideoTimingModes[VideoMode].Timing.HSyncWidth;
	MsaConfig->Vtm.Timing.HBackPorch =
			XVidC_VideoTimingModes[VideoMode].Timing.HBackPorch;
	MsaConfig->Vtm.Timing.HTotal =
			XVidC_VideoTimingModes[VideoMode].Timing.HTotal;
	MsaConfig->Vtm.Timing.HSyncPolarity =
			XVidC_VideoTimingModes[VideoMode].Timing.HSyncPolarity;
	MsaConfig->Vtm.Timing.VActive =
			XVidC_VideoTimingModes[VideoMode].Timing.VActive;
	MsaConfig->Vtm.Timing.F0PVFrontPorch =
			XVidC_VideoTimingModes[VideoMode].Timing.F0PVFrontPorch;
	MsaConfig->Vtm.Timing.F0PVSyncWidth =
			XVidC_VideoTimingModes[VideoMode].Timing.F0PVSyncWidth;
	MsaConfig->Vtm.Timing.F0PVBackPorch =
			XVidC_VideoTimingModes[VideoMode].Timing.F0PVBackPorch;
	MsaConfig->Vtm.Timing.F0PVTotal =
			XVidC_VideoTimingModes[VideoMode].Timing.F0PVTotal;
	MsaConfig->Vtm.Timing.F1VFrontPorch =
			XVidC_VideoTimingModes[VideoMode].Timing.F1VFrontPorch;
	MsaConfig->Vtm.Timing.F1VSyncWidth =
			XVidC_VideoTimingModes[VideoMode].Timing.F1VSyncWidth;
	MsaConfig->Vtm.Timing.F1VBackPorch =
			XVidC_VideoTimingModes[VideoMode].Timing.F1VBackPorch;
	MsaConfig->Vtm.Timing.F1VTotal =
			XVidC_VideoTimingModes[VideoMode].Timing.F1VTotal;
	MsaConfig->Vtm.Timing.VSyncPolarity =
			XVidC_VideoTimingModes[VideoMode].Timing.VSyncPolarity;

	/* Calculate the pixel clock frequency. */
	MsaConfig->PixelClockHz =
			XVidC_GetPixelClockHzByVmId(MsaConfig->Vtm.VmId);

	/* Calculate the rest of the MSA values. */
	XDp_TxCfgMsaRecalculate(InstancePtr, Stream);
}

/******************************************************************************/
/**
 * This function sets the main stream attribute values in the configuration
 * structure to match the preferred timing of the sink monitor. This Preferred
 * Timing Mode (PTM) information is stored in the sink's Extended Display
 * Identification Data (EDID).
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which the MSA values will be
 *		used for.
 * @param	Edid is a pointer to the Edid to use for the specified stream.
 *
 * @return	None.
 *
 * @note	The InstancePtr->TxInstance.MsaConfig structure is modified to
 *		reflect the main stream attribute values associated to the
 *		preferred timing of the sink monitor.
 *
*******************************************************************************/
void XDp_TxCfgMsaUseEdidPreferredTiming(XDp *InstancePtr, u8 Stream, u8 *Edid)
{
	XDp_TxMainStreamAttributes *MsaConfig;
	u8 *Ptm;

	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_TX);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
						(Stream == XDP_TX_STREAM_ID2) ||
						(Stream == XDP_TX_STREAM_ID3) ||
						(Stream == XDP_TX_STREAM_ID4));
	Xil_AssertVoid(Edid != NULL);

	MsaConfig = &InstancePtr->TxInstance.MsaConfig[Stream - 1];
	Ptm = &Edid[XDP_EDID_PTM];

	/* Configure the MSA values with the PTM information as
	 * specified by the preferred Detailed Timing Descriptor (DTD) of the
	 * monitor's EDID.
	 * Note, the PTM is only required for EDID versions 1.3 a newer. Earlier
	 * versions may not contain this information. */
	u16 HBlank = ((Ptm[XDP_EDID_DTD_HRES_HBLANK_U4] &
			XDP_EDID_DTD_XRES_XBLANK_U4_XBLANK_MASK) << 8) |
			Ptm[XDP_EDID_DTD_HBLANK_LSB];

	u16 VBlank = ((Ptm[XDP_EDID_DTD_VRES_VBLANK_U4] &
			XDP_EDID_DTD_XRES_XBLANK_U4_XBLANK_MASK) << 8) |
			Ptm[XDP_EDID_DTD_VBLANK_LSB];

	MsaConfig->Vtm.Timing.HActive =
			(((Ptm[XDP_EDID_DTD_HRES_HBLANK_U4] &
			XDP_EDID_DTD_XRES_XBLANK_U4_XRES_MASK) >>
			XDP_EDID_DTD_XRES_XBLANK_U4_XRES_SHIFT) << 8) |
			Ptm[XDP_EDID_DTD_HRES_LSB];

	MsaConfig->Vtm.Timing.VActive =
			(((Ptm[XDP_EDID_DTD_VRES_VBLANK_U4] &
			XDP_EDID_DTD_XRES_XBLANK_U4_XRES_MASK) >>
			XDP_EDID_DTD_XRES_XBLANK_U4_XRES_SHIFT) << 8) |
			Ptm[XDP_EDID_DTD_VRES_LSB];

	MsaConfig->PixelClockHz = (((Ptm[XDP_EDID_DTD_PIXEL_CLK_KHZ_MSB] <<
		8) | Ptm[XDP_EDID_DTD_PIXEL_CLK_KHZ_LSB]) * 10) * 1000;

	MsaConfig->Vtm.Timing.HFrontPorch =
			(((Ptm[XDP_EDID_DTD_XFPORCH_XSPW_U2] &
			XDP_EDID_DTD_XFPORCH_XSPW_U2_HFPORCH_MASK) >>
			XDP_EDID_DTD_XFPORCH_XSPW_U2_HFPORCH_SHIFT) << 8) |
			Ptm[XDP_EDID_DTD_HFPORCH_LSB];

	MsaConfig->Vtm.Timing.HSyncWidth =
			(((Ptm[XDP_EDID_DTD_XFPORCH_XSPW_U2] &
			XDP_EDID_DTD_XFPORCH_XSPW_U2_HSPW_MASK) >>
			XDP_EDID_DTD_XFPORCH_XSPW_U2_HSPW_SHIFT) << 8) |
			Ptm[XDP_EDID_DTD_HSPW_LSB];

	MsaConfig->Vtm.Timing.F0PVFrontPorch =
			(((Ptm[XDP_EDID_DTD_XFPORCH_XSPW_U2] &
			XDP_EDID_DTD_XFPORCH_XSPW_U2_VFPORCH_MASK) >>
			XDP_EDID_DTD_XFPORCH_XSPW_U2_VFPORCH_SHIFT) << 8) |
			((Ptm[XDP_EDID_DTD_VFPORCH_VSPW_L4] &
			XDP_EDID_DTD_VFPORCH_VSPW_L4_VFPORCH_MASK) >>
			XDP_EDID_DTD_VFPORCH_VSPW_L4_VFPORCH_SHIFT);

	MsaConfig->Vtm.Timing.F0PVSyncWidth =
			((Ptm[XDP_EDID_DTD_XFPORCH_XSPW_U2] &
			XDP_EDID_DTD_XFPORCH_XSPW_U2_VSPW_MASK) << 8) |
			(Ptm[XDP_EDID_DTD_VFPORCH_VSPW_L4] &
			XDP_EDID_DTD_VFPORCH_VSPW_L4_VSPW_MASK);

	/* Compute video mode timing values. */
	MsaConfig->Vtm.Timing.HBackPorch = HBlank -
					(MsaConfig->Vtm.Timing.HFrontPorch +
					MsaConfig->Vtm.Timing.HSyncWidth);

	MsaConfig->Vtm.Timing.F0PVBackPorch = VBlank -
					(MsaConfig->Vtm.Timing.F0PVFrontPorch +
					MsaConfig->Vtm.Timing.F0PVSyncWidth);

	MsaConfig->Vtm.Timing.HTotal = (MsaConfig->Vtm.Timing.HSyncWidth +
					MsaConfig->Vtm.Timing.HFrontPorch +
					MsaConfig->Vtm.Timing.HActive +
					MsaConfig->Vtm.Timing.HBackPorch);

	MsaConfig->Vtm.Timing.F0PVTotal = (MsaConfig->Vtm.Timing.F0PVSyncWidth +
					MsaConfig->Vtm.Timing.F0PVFrontPorch +
					MsaConfig->Vtm.Timing.VActive +
					MsaConfig->Vtm.Timing.F0PVBackPorch);

	MsaConfig->Vtm.FrameRate = MsaConfig->PixelClockHz /
					(MsaConfig->Vtm.Timing.HTotal *
					MsaConfig->Vtm.Timing.F0PVTotal);

	MsaConfig->Vtm.VmId = XVIDC_VM_USE_EDID_PREFERRED;

	/* Calculate the rest of the MSA values. */
	XDp_TxCfgMsaRecalculate(InstancePtr, Stream);
}

/******************************************************************************/
/**
 * This function takes a the main stream attributes from MsaConfigCustom and
 * copies them into InstancePtr->TxInstance.MsaConfig. If desired, given a base
 * set of attributes, the rest of the attributes may be derived. The minimal
 * required main stream attributes (MSA) that must be contained in the
 * MsaConfigCustom structure are:
 *	- Pixel clock (in Hz)
 *	- Frame rate
 *	- Horizontal active resolution
 *	- Horizontal front porch
 *	- Horizontal sync pulse width
 *	- Horizontal back porch
 *	- Horizontal total
 *	- Horizontal sync polarity
 *	- Vertical active resolution
 *	- Vertical back porch
 *	- Vertical sync pulse width
 *	- Vertical front porch
 *	- Vertical total
 *	- Vertical sync polarity
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which the MSA values will be
 *		used for.
 * @param	MsaConfigCustom is the structure that will be used to copy the
 *		main stream attributes from (into
 *		InstancePtr->TxInstance.MsaConfig).
 * @param	Recalculate is a boolean enable that determines whether or not
 *		the main stream attributes should be recalculated.
 *
 * @return	None.
 *
 * @note	The InstancePtr->TxInstance.MsaConfig structure is modified with
 *		the new values.
 *
*******************************************************************************/
void XDp_TxCfgMsaUseCustom(XDp *InstancePtr, u8 Stream,
		XDp_TxMainStreamAttributes *MsaConfigCustom, u8 Recalculate)
{
	XDp_TxMainStreamAttributes *MsaConfig;

	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_TX);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
						(Stream == XDP_TX_STREAM_ID2) ||
						(Stream == XDP_TX_STREAM_ID3) ||
						(Stream == XDP_TX_STREAM_ID4));
	Xil_AssertVoid(MsaConfigCustom != NULL);

	MsaConfig = &InstancePtr->TxInstance.MsaConfig[Stream - 1];

	/* Copy the MSA values from the user configuration structure. */
	MsaConfig->PixelClockHz = MsaConfigCustom->PixelClockHz;
	MsaConfig->Vtm.VmId = MsaConfigCustom->Vtm.VmId;
	MsaConfig->Vtm.FrameRate = MsaConfigCustom->Vtm.FrameRate;
	MsaConfig->Vtm.Timing.HActive =
				MsaConfigCustom->Vtm.Timing.HActive;
	MsaConfig->Vtm.Timing.HFrontPorch =
				MsaConfigCustom->Vtm.Timing.HFrontPorch;
	MsaConfig->Vtm.Timing.HSyncWidth =
				MsaConfigCustom->Vtm.Timing.HSyncWidth;
	MsaConfig->Vtm.Timing.HBackPorch =
				MsaConfigCustom->Vtm.Timing.HBackPorch;
	MsaConfig->Vtm.Timing.HTotal =
				MsaConfigCustom->Vtm.Timing.HTotal;
	MsaConfig->Vtm.Timing.HSyncPolarity =
				MsaConfigCustom->Vtm.Timing.HSyncPolarity;
	MsaConfig->Vtm.Timing.VActive =
				MsaConfigCustom->Vtm.Timing.VActive;
	MsaConfig->Vtm.Timing.F0PVFrontPorch =
				MsaConfigCustom->Vtm.Timing.F0PVFrontPorch;
	MsaConfig->Vtm.Timing.F0PVSyncWidth =
				MsaConfigCustom->Vtm.Timing.F0PVSyncWidth;
	MsaConfig->Vtm.Timing.F0PVBackPorch =
				MsaConfigCustom->Vtm.Timing.F0PVBackPorch;
	MsaConfig->Vtm.Timing.F0PVTotal =
				MsaConfigCustom->Vtm.Timing.F0PVTotal;
	MsaConfig->Vtm.Timing.F1VFrontPorch =
				MsaConfigCustom->Vtm.Timing.F1VFrontPorch;
	MsaConfig->Vtm.Timing.F1VSyncWidth =
				MsaConfigCustom->Vtm.Timing.F1VSyncWidth;
	MsaConfig->Vtm.Timing.F1VBackPorch =
				MsaConfigCustom->Vtm.Timing.F1VBackPorch;
	MsaConfig->Vtm.Timing.F1VTotal =
				MsaConfigCustom->Vtm.Timing.F1VTotal;
	MsaConfig->Vtm.Timing.VSyncPolarity =
				MsaConfigCustom->Vtm.Timing.VSyncPolarity;

	if (Recalculate) {
		/* Calculate the rest of the MSA values. */
		XDp_TxCfgMsaRecalculate(InstancePtr, Stream);
	}
	else {
		/* Use the custom values for the rest. */
		MsaConfig->TransferUnitSize = MsaConfigCustom->TransferUnitSize;
		MsaConfig->UserPixelWidth = MsaConfigCustom->UserPixelWidth;
		MsaConfig->NVid = MsaConfigCustom->NVid;
		MsaConfig->HStart = MsaConfigCustom->HStart;
		MsaConfig->VStart = MsaConfigCustom->VStart;
		MsaConfig->Misc0 = MsaConfigCustom->Misc0;
		MsaConfig->Misc1 = MsaConfigCustom->Misc1;
		MsaConfig->DataPerLane = MsaConfigCustom->DataPerLane;
		MsaConfig->AvgBytesPerTU = MsaConfigCustom->AvgBytesPerTU;
		MsaConfig->InitWait = MsaConfigCustom->InitWait;
	}
}

/******************************************************************************/
/**
 * This function sets the bits per color value of the video stream.
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which to set the color depth.
 * @param	BitsPerColor is the new number of bits per color to use.
 *
 * @return	None.
 *
 * @note	The InstancePtr->TxInstance.MsaConfig structure is modified to
 *		reflect the new main stream attributes associated with a new
 *		bits per color value.
 *
*******************************************************************************/
void XDp_TxCfgMsaSetBpc(XDp *InstancePtr, u8 Stream, u8 BitsPerColor)
{
	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_TX);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
						(Stream == XDP_TX_STREAM_ID2) ||
						(Stream == XDP_TX_STREAM_ID3) ||
						(Stream == XDP_TX_STREAM_ID4));
	Xil_AssertVoid((BitsPerColor == 6) || (BitsPerColor == 8) ||
				(BitsPerColor == 10) || (BitsPerColor == 12) ||
				(BitsPerColor == 16));

	InstancePtr->TxInstance.MsaConfig[Stream - 1].BitsPerColor =
								BitsPerColor;

	/* Calculate the rest of the MSA values. */
	XDp_TxCfgMsaRecalculate(InstancePtr, Stream);
}

/******************************************************************************/
/**
 * This function enables or disables synchronous clock mode for a video stream.
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which to enable or disable
 *		synchronous clock mode.
 * @param	Enable if set to 1, will enable synchronous clock mode.
 *		Otherwise, if set to 0, synchronous clock mode will be disabled.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
void XDp_TxCfgMsaEnSynchClkMode(XDp *InstancePtr, u8 Stream, u8 Enable)
{
	XDp_TxMainStreamAttributes *MsaConfig;

	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_TX);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
		(Stream == XDP_TX_STREAM_ID2) ||
		(Stream == XDP_TX_STREAM_ID3) ||
		(Stream == XDP_TX_STREAM_ID4));
	Xil_AssertVoid((Enable == 0) || (Enable == 1));

	MsaConfig = &InstancePtr->TxInstance.MsaConfig[Stream - 1];

        MsaConfig->SynchronousClockMode = Enable;

	if (Enable == 1) {
		MsaConfig->Misc0 |= (1 <<
			XDP_TX_MAIN_STREAMX_MISC0_COMPONENT_FORMAT_SHIFT);
	}
	else {
		MsaConfig->Misc0 &= ~(1 <<
			XDP_TX_MAIN_STREAMX_MISC0_COMPONENT_FORMAT_SHIFT);
	}
}

/******************************************************************************/
/**
 * This function clears the main stream attributes registers of the DisplayPort
 * TX core and sets them to the values specified in the main stream attributes
 * configuration structure.
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which to set the MSA values for.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
void XDp_TxSetVideoMode(XDp *InstancePtr, u8 Stream)
{
	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
						(Stream == XDP_TX_STREAM_ID2) ||
						(Stream == XDP_TX_STREAM_ID3) ||
						(Stream == XDP_TX_STREAM_ID4));

	XDp_TxClearMsaValues(InstancePtr, Stream);
	XDp_TxSetMsaValues(InstancePtr, Stream);
}

/******************************************************************************/
/**
 * This function clears the main stream attributes registers of the DisplayPort
 * TX core.
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which to clear the MSA values.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
void XDp_TxClearMsaValues(XDp *InstancePtr, u8 Stream)
{
	XDp_Config *Config;
	u32 StreamOffset[4] = {0, XDP_TX_STREAM2_MSA_START_OFFSET,
					XDP_TX_STREAM3_MSA_START_OFFSET,
					XDP_TX_STREAM4_MSA_START_OFFSET};

	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_TX);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
						(Stream == XDP_TX_STREAM_ID2) ||
						(Stream == XDP_TX_STREAM_ID3) ||
						(Stream == XDP_TX_STREAM_ID4));

	Config = &InstancePtr->Config;

	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_HTOTAL +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_VTOTAL +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_POLARITY +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_HSWIDTH +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_VSWIDTH +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_HRES +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_VRES +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_HSTART +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_VSTART +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_MISC0 +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MAIN_STREAM_MISC1 +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_USER_PIXEL_WIDTH +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_USER_DATA_COUNT_PER_LANE +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_M_VID +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_N_VID +
						StreamOffset[Stream - 1], 0);

        XDp_WriteReg(Config->BaseAddr, XDP_TX_STREAM1 + (Stream - 1) * 4, 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_TU_SIZE +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_MIN_BYTES_PER_TU +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_FRAC_BYTES_PER_TU +
						StreamOffset[Stream - 1], 0);
	XDp_WriteReg(Config->BaseAddr, XDP_TX_INIT_WAIT +
						StreamOffset[Stream - 1], 0);
}

/******************************************************************************/
/**
 * This function sets the main stream attributes registers of the DisplayPort TX
 * core with the values specified in the main stream attributes configuration
 * structure.
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number for which to set the MSA values for.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
void XDp_TxSetMsaValues(XDp *InstancePtr, u8 Stream)
{
	XDp_Config *ConfigPtr;
	XDp_TxMainStreamAttributes *MsaConfig;
	u32 StreamOffset[4] = {0, XDP_TX_STREAM2_MSA_START_OFFSET,
					XDP_TX_STREAM3_MSA_START_OFFSET,
					XDP_TX_STREAM4_MSA_START_OFFSET};

	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_TX);
	Xil_AssertVoid((Stream == XDP_TX_STREAM_ID1) ||
						(Stream == XDP_TX_STREAM_ID2) ||
						(Stream == XDP_TX_STREAM_ID3) ||
						(Stream == XDP_TX_STREAM_ID4));

	ConfigPtr = &InstancePtr->Config;
	MsaConfig = &InstancePtr->TxInstance.MsaConfig[Stream - 1];

	/* Set the main stream attributes to the associated DisplayPort TX core
	 * registers. */
	if (InstancePtr->TxInstance.TxSetMsaCallback) {
		/* Callback for MSA value updates. */
		InstancePtr->TxInstance.TxSetMsaCallback(
				InstancePtr->TxInstance.TxMsaCallbackRef);
	}
	else {
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_HTOTAL +
			StreamOffset[Stream - 1], MsaConfig->Vtm.Timing.HTotal);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_VTOTAL +
			StreamOffset[Stream - 1],
			MsaConfig->Vtm.Timing.F0PVTotal);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_POLARITY +
			StreamOffset[Stream - 1],
			MsaConfig->Vtm.Timing.HSyncPolarity |
			(MsaConfig->Vtm.Timing.VSyncPolarity <<
			XDP_TX_MAIN_STREAMX_POLARITY_VSYNC_POL_SHIFT));
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_HSWIDTH +
			StreamOffset[Stream - 1],
			MsaConfig->Vtm.Timing.HSyncWidth);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_VSWIDTH +
			StreamOffset[Stream - 1],
			MsaConfig->Vtm.Timing.F0PVSyncWidth);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_HRES +
			StreamOffset[Stream - 1],
			MsaConfig->Vtm.Timing.HActive);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_VRES +
			StreamOffset[Stream - 1],
			MsaConfig->Vtm.Timing.VActive);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_HSTART +
			StreamOffset[Stream - 1], MsaConfig->HStart);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_VSTART +
			StreamOffset[Stream - 1], MsaConfig->VStart);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_MISC0 +
			StreamOffset[Stream - 1], MsaConfig->Misc0);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MAIN_STREAM_MISC1 +
			StreamOffset[Stream - 1], MsaConfig->Misc1);
		XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_USER_PIXEL_WIDTH +
			StreamOffset[Stream - 1], MsaConfig->UserPixelWidth);
	}
	XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_M_VID +
		StreamOffset[Stream - 1], MsaConfig->PixelClockHz / 1000);
	XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_N_VID +
		StreamOffset[Stream - 1], MsaConfig->NVid);
	XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_USER_DATA_COUNT_PER_LANE +
		StreamOffset[Stream - 1], MsaConfig->DataPerLane);

	/* Disable the end of line reset to the internal video pipe in case of
	 * 4K2K reduced blanking. */
	XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_LINE_RESET_DISABLE,
			(MsaConfig->Vtm.VmId == XVIDC_VM_4K2K_60_P_RB) ?
			XDP_TX_LINE_RESET_DISABLE_MASK : 0);

	/* Set the transfer unit values to the associated DisplayPort TX core
	 * registers. */
        if (InstancePtr->TxInstance.MstEnable == 1) {
                XDp_WriteReg(ConfigPtr->BaseAddr,
			XDP_TX_STREAM1 + (Stream - 1) * 4,
                        ((MsaConfig->AvgBytesPerTU / 1000) << 16) |
                        (MsaConfig->AvgBytesPerTU % 1000));
        }
	XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_TU_SIZE +
			StreamOffset[Stream - 1], MsaConfig->TransferUnitSize);
	XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_MIN_BYTES_PER_TU +
		StreamOffset[Stream - 1], MsaConfig->AvgBytesPerTU / 1000);
	XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_FRAC_BYTES_PER_TU +
		StreamOffset[Stream - 1],
		(MsaConfig->AvgBytesPerTU % 1000) * 1024 / 1000);
	XDp_WriteReg(ConfigPtr->BaseAddr, XDP_TX_INIT_WAIT +
			StreamOffset[Stream - 1], MsaConfig->InitWait);
}

/******************************************************************************/
/**
 * This function configures the number of pixels output through the user data
 * interface.
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	UserPixelWidth is the user pixel width to be configured.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
void XDp_RxSetUserPixelWidth(XDp *InstancePtr, u8 UserPixelWidth)
{
	/* Verify arguments. */
	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);
	Xil_AssertVoid(XDp_GetCoreType(InstancePtr) == XDP_RX);
	Xil_AssertVoid((UserPixelWidth == 1) || (UserPixelWidth == 2) ||
							(UserPixelWidth == 4));

	XDp_WriteReg(InstancePtr->Config.BaseAddr, XDP_RX_USER_PIXEL_WIDTH,
								UserPixelWidth);

	XDp_WriteReg(InstancePtr->Config.BaseAddr, XDP_RX_SOFT_RESET, 0x1);
	XDp_WriteReg(InstancePtr->Config.BaseAddr, XDP_RX_SOFT_RESET, 0x0);
}

/******************************************************************************/
/**
 * When the driver is in multi-stream transport (MST) mode, this function will
 * make the necessary calculations to describe a stream in MST mode. The key
 * values are the payload bandwidth number (PBN), the number of timeslots
 * required for allocating the bandwidth, and the average bytes per transfer
 * unit (both the integer and the fractional part).
 *
 * @param	InstancePtr is a pointer to the XDp instance.
 * @param	Stream is the stream number to make the calculations for.
 * @param	BitsPerPixel is the number of bits that is used to store one
 *		pixel.
 *
 * @return	None.
 *
 * @note	None.
 *
*******************************************************************************/
static void XDp_TxCalculateTs(XDp *InstancePtr, u8 Stream, u8 BitsPerPixel)
{
	XDp_TxMainStreamAttributes *MsaConfig =
				&InstancePtr->TxInstance.MsaConfig[Stream - 1];
	XDp_TxLinkConfig *LinkConfig = &InstancePtr->TxInstance.LinkConfig;
	double PeakPixelBw;
	u32 LinkBw;
	double Average_StreamSymbolTimeSlotsPerMTP;
	double Target_Average_StreamSymbolTimeSlotsPerMTP;
	double MaximumTarget_Average_StreamSymbolTimeSlotsPerMTP;
	u32 TsInt;
	u32 TsFrac;

	PeakPixelBw = ((double)MsaConfig->PixelClockHz / 1000000) *
						((double)BitsPerPixel / 8);
	LinkBw = (LinkConfig->LaneCount * LinkConfig->LinkRate * 27);

	/* Calculate the payload bandwidth number (PBN).  */
	InstancePtr->TxInstance.MstStreamConfig[Stream - 1].MstPbn =
					1.006 * PeakPixelBw * ((double)64 / 54);
	/* Ceil - round up if required, avoiding overhead of math.h. */
	if ((double)(1.006 * PeakPixelBw * ((double)64 / 54)) >
			((double)InstancePtr->TxInstance.MstStreamConfig[
			Stream - 1].MstPbn)) {
		InstancePtr->TxInstance.MstStreamConfig[Stream - 1].MstPbn++;
	}

	/* Calculate the average stream symbol time slots per MTP. */
	Average_StreamSymbolTimeSlotsPerMTP = (64.0 * PeakPixelBw / LinkBw);
	MaximumTarget_Average_StreamSymbolTimeSlotsPerMTP = (54.0 *
		((double)InstancePtr->TxInstance.MstStreamConfig[Stream - 1].
		MstPbn / LinkBw));

	/* The target value to be found needs to follow the condition:
	 *	Average_StreamSymbolTimeSlotsPerMTP <=
	 *		Target_Average_StreamSymbolTimeSlotsPerMTP
	 *	>= MaximumTarget_Average_StreamSymbolTimeSlotsPerMTP
	 * Obtain the greatest target value that satisfies the above condition
	 * and still a multiple of 1/TsFrac_Denominator.
	 * Note: TsFrac_Denominator = 8. */
	/* Round down. */
	Target_Average_StreamSymbolTimeSlotsPerMTP =
				(u32)Average_StreamSymbolTimeSlotsPerMTP;
	/* Find the greatest multiple that is less than the maximum. */
	Target_Average_StreamSymbolTimeSlotsPerMTP += ((1.0 / 8.0) * (u32)(8.0 *
			(MaximumTarget_Average_StreamSymbolTimeSlotsPerMTP -
			Target_Average_StreamSymbolTimeSlotsPerMTP)));

	/* Determine the integer and the fractional part of the number of time
	 * slots that will be allocated for the stream. */
	TsInt = Target_Average_StreamSymbolTimeSlotsPerMTP;
	TsFrac = (((double)Target_Average_StreamSymbolTimeSlotsPerMTP * 1000) -
								(TsInt * 1000));

	/* Store TsInt and TsFrac in AvgBytesPerTU. */
	MsaConfig->AvgBytesPerTU = TsInt * 1000 + TsFrac;

	/* Set the number of time slots to allocate for this stream. */
	MsaConfig->TransferUnitSize = TsInt;
	if (TsFrac != 0) {
		/* Round up. */
		MsaConfig->TransferUnitSize++;
	}
	if ((InstancePtr->Config.PayloadDataWidth == 4) &&
				(MsaConfig->TransferUnitSize % 4) != 0) {
		/* Set to a multiple of 4 boundary. */
		MsaConfig->TransferUnitSize += (4 -
					(MsaConfig->TransferUnitSize % 4));
	}
	else if ((MsaConfig->TransferUnitSize % 2) != 0) {
		/* Set to an even boundary. */
		MsaConfig->TransferUnitSize++;
	}

	/* Determine the PBN for the stream. */
	InstancePtr->TxInstance.MstStreamConfig[Stream - 1].MstPbn =
			MsaConfig->TransferUnitSize *
			(LinkConfig->LaneCount * LinkConfig->LinkRate / 2);
}
/** @} */
