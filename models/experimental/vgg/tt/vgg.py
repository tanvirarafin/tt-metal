# SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.

# SPDX-License-Identifier: Apache-2.0

import torch.nn as nn

from torchvision import models
from typing import List, Union, Dict, cast

import tt_lib

from tt_lib.fallback_ops import fallback_ops
from models.helper_funcs import Linear as TtLinear
from models.utility_functions import (
    is_conv_supported_on_device,
    run_conv_on_device_wrapper,
)
from models.experimental.vgg.vgg_utils import format_tensor


class TtVGG(nn.Module):
    def __init__(
        self,
        features: List,
        init_weights: bool = True,
        device=None,
        base_address="",
        tt_cache_path=None,
        tt_dtype=tt_lib.tensor.DataType.BFLOAT16,
    ) -> None:
        super().__init__()
        assert init_weights == False, "we are loading weights, not initializing them"
        self.device = device
        self.base_address = base_address
        self.features = features
        self.avgpool = fallback_ops.AdaptiveAvgPool2d((7, 7))

        self.output_mem_config = tt_lib.tensor.MemoryConfig(
            tt_lib.tensor.TensorMemoryLayout.INTERLEAVED, tt_lib.tensor.BufferType.L1
        )

        linear1_weight = tt_lib.tensor.load_tensor(f"{tt_cache_path}classifier.0.weight{tt_dtype}.bin")
        linear1_bias = tt_lib.tensor.load_tensor(f"{tt_cache_path}classifier.0.bias{tt_dtype}.bin")

        linear2_weight = tt_lib.tensor.load_tensor(f"{tt_cache_path}classifier.3.weight{tt_dtype}.bin")
        linear2_bias = tt_lib.tensor.load_tensor(f"{tt_cache_path}classifier.3.bias{tt_dtype}.bin")

        linear3_weight = tt_lib.tensor.load_tensor(f"{tt_cache_path}classifier.6.weight{tt_dtype}.bin")
        linear3_bias = tt_lib.tensor.load_tensor(f"{tt_cache_path}classifier.6.bias{tt_dtype}.bin")

        linear1 = TtLinear(
            in_features=linear1_weight.shape()[-1],
            out_features=linear1_weight.shape()[-2],
            weight=linear1_weight,
            bias=linear1_bias,
            output_mem_config=self.output_mem_config,
        )

        linear2 = TtLinear(
            in_features=linear2_weight.shape()[-1],
            out_features=linear2_weight.shape()[-2],
            weight=linear2_weight,
            bias=linear2_bias,
            output_mem_config=self.output_mem_config,
        )

        linear3 = TtLinear(
            in_features=linear3_weight.shape()[-1],
            out_features=linear3_weight.shape()[-2],
            weight=linear3_weight,
            bias=linear3_bias,
            output_mem_config=self.output_mem_config,
        )

        self.classifier = [
            linear1,
            tt_lib.tensor.relu,
            linear2,
            tt_lib.tensor.relu,
            linear3,
        ]

    def forward(self, tt_x: tt_lib.tensor.Tensor) -> tt_lib.tensor.Tensor:
        for layer in self.features:
            if layer is tt_lib.tensor.relu:
                tt_x = layer(tt_x, output_mem_config=self.output_mem_config)
            else:
                tt_x = layer(tt_x)

        batch, c, w, h = tt_x.shape()
        tt_x = self.avgpool(tt_x)

        tt_x = fallback_ops.reshape(tt_x, batch, 1, 1, c * w * h)
        tt_x = format_tensor(tt_x, tt_lib.tensor.Layout.TILE, self.device, self.output_mem_config)
        for layer in self.classifier:
            tt_x = layer(tt_x)

        return tt_x


def make_layers(
    cfg: List[Union[str, int]],
    batch_norm: bool = False,
    base_address="features",
    device=None,
    disable_conv_on_tt_device=True,
    tt_cache_path=None,
    tt_dtype=tt_lib.tensor.DataType.BFLOAT16,
) -> nn.Sequential:
    layers: List = []
    in_channels = 3

    for v in cfg:
        if v == "M":
            layers += [fallback_ops.MaxPool2d(kernel_size=2, stride=2)]

        else:
            v = cast(int, v)
            if not batch_norm:
                ind = len(layers)
                conv2d_params = [v, in_channels, 3, 3, 1, 1, 1, 1, 1, 1]
                if not disable_conv_on_tt_device and is_conv_supported_on_device(conv2d_params):
                    assert device is not None
                    conv2d_weight = tt_lib.tensor.load_tensor(
                        f"{tt_cache_path}{base_address}.{ind}.weight{tt_dtype}.bin"
                    )
                    conv2d_bias = tt_lib.tensor.load_tensor(
                        f"{tt_cache_path}{base_address}.{ind}.bias{tt_dtype}.bin"
                    ).tolist()

                    conv2d = run_conv_on_device_wrapper(
                        conv2d_weight.reshape(-1).tolist(),
                        conv2d_params,
                        device,
                        conv2d_bias,
                    )
                else:
                    weight = tt_lib.tensor.load_tensor(f"{tt_cache_path}{base_address}.{ind}.weight{tt_dtype}.bin")
                    bias = tt_lib.tensor.load_tensor(f"{tt_cache_path}{base_address}.{ind}.bias{tt_dtype}.bin")
                    conv2d = fallback_ops.Conv2d(
                        weights=weight,
                        biases=bias,
                        in_channels=in_channels,
                        out_channels=v,
                        kernel_size=3,
                        padding=1,
                    )

                layers += [conv2d, tt_lib.tensor.relu]
            else:
                assert False, "we do not support batchnorm"
                layers += [conv2d, nn.BatchNorm2d(v), nn.ReLU(inplace=True)]
            in_channels = v

    return layers


cfgs: Dict[str, List[Union[str, int]]] = {
    "A": [64, "M", 128, "M", 256, 256, "M", 512, 512, "M", 512, 512, "M"],
    "B": [64, 64, "M", 128, 128, "M", 256, 256, "M", 512, 512, "M", 512, 512, "M"],
    "D": [64, 64, "M", 128, 128, "M", 256, 256, 256, "M", 512, 512, 512, "M", 512, 512, 512, "M"],
    "E": [64, 64, "M", 128, 128, "M", 256, 256, 256, 256, "M", 512, 512, 512, 512, "M", 512, 512, 512, 512, "M"],
}


def _vgg(features, init_weights, device, base_address="", tt_cache_path=None, tt_dtype=tt_lib.tensor.DataType.BFLOAT16):
    return TtVGG(
        features,
        init_weights=init_weights,
        device=device,
        base_address=base_address,
        tt_cache_path=tt_cache_path,
        tt_dtype=tt_dtype,
    )


def vgg16(
    device, disable_conv_on_tt_device=True, tt_cache_path=None, tt_dtype=tt_lib.tensor.DataType.BFLOAT16
) -> TtVGG:
    torch_vgg = models.vgg16(weights=models.VGG16_Weights.IMAGENET1K_V1)
    torch_vgg.eval()
    model = _vgg(
        make_layers(
            cfgs["D"],
            batch_norm=False,
            device=device,
            disable_conv_on_tt_device=disable_conv_on_tt_device,
            tt_cache_path=tt_cache_path,
            tt_dtype=tt_dtype,
        ),
        init_weights=False,
        device=device,
        tt_cache_path=tt_cache_path,
        tt_dtype=tt_dtype,
    )
    return model


def vgg11(
    device, disable_conv_on_tt_device=True, tt_cache_path=None, tt_dtype=tt_lib.tensor.DataType.BFLOAT16
) -> TtVGG:
    torch_vgg = models.vgg11(weights=models.VGG11_Weights.IMAGENET1K_V1)
    torch_vgg.eval()
    model = _vgg(
        make_layers(
            cfgs["A"],
            batch_norm=False,
            device=device,
            disable_conv_on_tt_device=disable_conv_on_tt_device,
            tt_cache_path=tt_cache_path,
            tt_dtype=tt_dtype,
        ),
        init_weights=False,
        device=device,
        tt_cache_path=tt_cache_path,
        tt_dtype=tt_dtype,
    )
    return model
