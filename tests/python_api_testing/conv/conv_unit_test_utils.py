import pytest
from pathlib import Path
import sys

f = f"{Path(__file__).parent}"
sys.path.append(f"{f}/../..")
import torch
import tt_lib as ttl
from tt_lib.utils import _nearest_32, _nearest_y

def create_conv_act_tensor(torch_tensor, N, C, H, W):
    torch_tensor = torch.permute(torch_tensor, (0, 2, 3, 1))
    act_shape_channel_padded = [N, H, W, _nearest_y(C, 16)]
    tt_tensor = ttl.tensor.Tensor(torch_tensor, ttl.tensor.DataType.BFLOAT16)
    tt_tensor = tt_tensor.pad(act_shape_channel_padded, (0,0,0,0), 0.0)
    return tt_tensor

def create_conv_weight_tensor(torch_tensor, K, C, R, S, in1_block_h, in1_block_w):
    weights_shape = [K,C,R,S]
    weights_channels_padded_shape = [_nearest_32(K),_nearest_y(C, 16),R,S]
    B_ = ttl.tensor.Tensor(
        torch.flatten(torch_tensor).tolist(),
        weights_shape,
        ttl.tensor.DataType.BFLOAT16,
        ttl.tensor.Layout.ROW_MAJOR
    ).pad(weights_channels_padded_shape, (0,0,0,0), 0.0)
    B_tiled_host = ttl.tensor.convert_conv_weight_tensor_to_tiled_layout(B_, in1_block_h, in1_block_w)
    return B_tiled_host
