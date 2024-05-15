// SPDX-License-Identifier: MIT
/*
 * Copyright © 2023 Intel Corporation
 */

#include <stdint.h>
#include <stddef.h>

#include "intel_chipset.h"
#include "intel_cmds_info.h"

#define BLT_INFO(_cmd, _tiling)  { \
		.blt_cmd_type = _cmd, \
		.supported_tiling = _tiling \
	}

#define BLT_INFO_EXT(_cmd, _tiling, _flags)  { \
		.blt_cmd_type = _cmd, \
		.supported_tiling = _tiling, \
		.flags = _flags, \
	}

#define TILE_4		BIT(T_TILE4)
#define TILE_64		BIT(T_TILE64)
#define TILE_L		BIT(T_LINEAR)
#define TILE_X		BIT(T_XMAJOR)
#define TILE_Y		BIT(T_YMAJOR)
#define TILE_Yf		BIT(T_YFMAJOR)

#define TILE_L_4_64	(TILE_L | TILE_4 | TILE_64)
#define TILE_L_X	(TILE_L | TILE_X)
#define TILE_L_X_Y	(TILE_L | TILE_X | TILE_Y)
#define TILE_L_X_4_64	(TILE_L | TILE_X | TILE_4 | TILE_64)
#define TILE_L_Y	(TILE_L | TILE_Y)
#define TILE_L_Y_4_64	(TILE_L | TILE_Y | TILE_4 | TILE_64)
#define TILE_L_Y_Yf_64	(TILE_L | TILE_Y | TILE_Yf | TILE_64)

static const struct blt_cmd_info src_copy = BLT_INFO(SRC_COPY, TILE_L);
static const struct blt_cmd_info
		pre_gen6_xy_src_copy = BLT_INFO(XY_SRC_COPY, TILE_L_X);

static const struct blt_cmd_info
		gen6_xy_src_copy = BLT_INFO(XY_SRC_COPY, TILE_L_X_Y);

static const struct blt_cmd_info
		gen11_xy_fast_copy = BLT_INFO(XY_FAST_COPY, TILE_L_Y_Yf_64);

static const struct blt_cmd_info
		gen12_xy_fast_copy = BLT_INFO(XY_FAST_COPY, TILE_L_Y_4_64);

static const struct blt_cmd_info
		dg2_xy_fast_copy = BLT_INFO(XY_FAST_COPY, TILE_L_X_4_64);

static const struct blt_cmd_info
		pvc_xy_fast_copy = BLT_INFO(XY_FAST_COPY, TILE_L_4_64);

static const struct blt_cmd_info
		gen12_xy_block_copy = BLT_INFO(XY_BLOCK_COPY, TILE_L_Y);

static const struct blt_cmd_info
		dg2_xy_block_copy = BLT_INFO_EXT(XY_BLOCK_COPY, TILE_L_X_4_64,
						 BLT_CMD_EXTENDED |
						 BLT_CMD_SUPPORTS_COMPRESSION);

static const struct blt_cmd_info
		xe2_xy_block_copy = BLT_INFO_EXT(XY_BLOCK_COPY, TILE_L_X_4_64,
						 BLT_CMD_EXTENDED |
						 BLT_CMD_SUPPORTS_COMPRESSION);

static const struct blt_cmd_info
		mtl_xy_block_copy = BLT_INFO_EXT(XY_BLOCK_COPY, TILE_L_X_4_64,
						 BLT_CMD_EXTENDED);

static const struct blt_cmd_info
		pvc_xy_block_copy = BLT_INFO_EXT(XY_BLOCK_COPY, TILE_L_4_64,
						 BLT_CMD_EXTENDED);

static const struct blt_cmd_info
		pvc_mem_copy = BLT_INFO(MEM_COPY,
					BIT(M_LINEAR) |
					BIT(M_MATRIX));

static const struct blt_cmd_info
		pvc_mem_set = BLT_INFO(MEM_SET,
				       BIT(M_LINEAR) |
				       BIT(M_MATRIX));

static const struct blt_cmd_info
		pre_gen6_xy_color_blt = BLT_INFO(XY_COLOR_BLT, TILE_L_X);

static const struct blt_cmd_info
		gen6_xy_color_blt = BLT_INFO_EXT(XY_COLOR_BLT, TILE_L_X_Y,
						 BLT_CMD_EXTENDED);


const struct intel_cmds_info pre_gen6_cmds_info = {
	.blt_cmds = {
		[SRC_COPY] = &src_copy,
		[XY_SRC_COPY] = &pre_gen6_xy_src_copy,
		[XY_COLOR_BLT] = &pre_gen6_xy_color_blt,
	}
};

const struct intel_cmds_info gen6_cmds_info =  {
	.blt_cmds = {
		[SRC_COPY] = &src_copy,
		[XY_SRC_COPY] = &gen6_xy_src_copy,
		[XY_COLOR_BLT] = &gen6_xy_color_blt,
	}
};

const struct intel_cmds_info gen8_cmds_info = {
	.blt_cmds = {
		[XY_SRC_COPY] = &gen6_xy_src_copy,
		[XY_COLOR_BLT] = &gen6_xy_color_blt,
	}
};

const struct intel_cmds_info gen11_cmds_info = {
	.blt_cmds = {
		[XY_SRC_COPY] = &gen6_xy_src_copy,
		[XY_FAST_COPY] = &gen11_xy_fast_copy,
		[XY_COLOR_BLT] = &gen6_xy_color_blt,
	}
};

const struct intel_cmds_info gen12_cmds_info = {
	.blt_cmds = {
		[XY_SRC_COPY] = &gen6_xy_src_copy,
		[XY_FAST_COPY] = &gen12_xy_fast_copy,
		[XY_BLOCK_COPY] = &gen12_xy_block_copy,
		[XY_COLOR_BLT] = &gen6_xy_color_blt,
	}
};

const struct intel_cmds_info gen12_dg2_cmds_info = {
	.blt_cmds = {
		[XY_SRC_COPY] = &gen6_xy_src_copy,
		[XY_FAST_COPY] = &dg2_xy_fast_copy,
		[XY_BLOCK_COPY] = &dg2_xy_block_copy,
		[XY_COLOR_BLT] = &gen6_xy_color_blt,
	}
};

const struct intel_cmds_info gen12_mtl_cmds_info = {
	.blt_cmds = {
		[XY_FAST_COPY] = &dg2_xy_fast_copy,
		[XY_BLOCK_COPY] = &mtl_xy_block_copy,
	}
};

const struct intel_cmds_info gen12_pvc_cmds_info = {
	.blt_cmds = {
		[XY_FAST_COPY] = &pvc_xy_fast_copy,
		[XY_BLOCK_COPY] = &pvc_xy_block_copy,
		[MEM_COPY] = &pvc_mem_copy,
		[MEM_SET] = &pvc_mem_set,
	}
};

const struct intel_cmds_info xe2_cmds_info  = {
	.blt_cmds = {
		[XY_FAST_COPY] = &dg2_xy_fast_copy,
		[XY_BLOCK_COPY] = &xe2_xy_block_copy,
		[MEM_COPY] = &pvc_mem_copy,
		[MEM_SET] = &pvc_mem_set,
	}
};

const struct blt_cmd_info *blt_get_cmd_info(const struct intel_cmds_info *cmds_info,
					    enum blt_cmd_type cmd)
{
	if (!cmds_info)
		return NULL;

	return cmds_info->blt_cmds[cmd];
}
