from typing import List, Union
from .. import tensor
#from libs.tt_lib.utils import _nearest_32
from ..utils import _nearest_32, _nearest_y


def conv(weight: List[Union[int, float]], conv_params, device, bias=None):
    """
    Returns a function that performs a Convolution.
    bias is optional. If provided, it must be in tiled layout
    """
    assert(len(conv_params) == 10)
    K, C, R, S, U, V, P_H, P_W, dilation, groups = [conv_params[i] for i in range(10)]
    # Hardcode block sizes
    act_block_h = 4
    act_block_w = 4
    weight_block_h = act_block_w
    weight_block_w = 4
    out_subblock_h = 4
    out_subblock_w = 2
    if dilation != 1 or groups != 1:
        return None
    weights_shape = [K,C,R,S]
    weights_channels_padded_shape = [_nearest_32(K),_nearest_32(C),R,S]
    weight_untiled = tensor.Tensor(
        weight,
        weights_shape,
        tensor.DataType.BFLOAT16,
        tensor.Layout.ROW_MAJOR
    ).pad(weights_channels_padded_shape, (0,0,0,0), 0)
    weight_tiled_ = tensor.convert_conv_weight_tensor_to_tiled_layout(weight_untiled, weight_block_h, weight_block_w)
    weight_on_device = weight_tiled_.to(device, tensor.MemoryConfig(False, 0))
    if bias is None:
        bias_on_device = None
    else:
        bias_shape = [1,1,1,K]
        bias_channels_padded_shape = [1, 1, 1, _nearest_32(K)]
        bias_ = tensor.Tensor(
            bias,
            bias_shape,
            tensor.DataType.BFLOAT16,
            tensor.Layout.ROW_MAJOR
        ).pad(bias_channels_padded_shape, (0,0,0,0), 0)
        bias_on_device = bias_.to(device)

    def conv_(activation):
        [_,_,H,W] = activation.shape()
        OH = ((int) ((H - R + 2 * P_H) / U)) + 1
        OW = ((int) ((W - S + 2 * P_W) / V)) + 1
        conv_as_mm_output_shape = [1,1,_nearest_y(OH*OW, 32*act_block_h),_nearest_y(K, 32*weight_block_w)]
        output = tensor.conv(activation, weight_on_device, [R,S,U,V,P_H,P_W], act_block_h, act_block_w, weight_block_w, out_subblock_h, out_subblock_w)

        assert(output.shape() == conv_as_mm_output_shape)

        if bias_on_device is not None:
            print(str(output.shape()))
            print(str(bias_on_device.shape()))
            output_plus_bias = tensor.bcast(output, bias_on_device, tensor.BcastOpMath.ADD, tensor.BcastOpDim.H)
            return output_plus_bias

        return output

    return conv_
